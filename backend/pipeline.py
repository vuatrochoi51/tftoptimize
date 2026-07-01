import requests
import sqlite3
import os
import json
from pathlib import Path

BASE_URL = "https://raw.communitydragon.org/latest/cdragon/tft/vi_vn.json"
IMAGE_BASE_URL = "https://raw.communitydragon.org/latest/game/"
DB_PATH = "tft_meta.db"
ASSETS_DIR = Path("assets")

ASSETS_DIR.mkdir(parents=True, exist_ok=True)
(ASSETS_DIR / "champions").mkdir(exist_ok=True)
(ASSETS_DIR / "items").mkdir(exist_ok=True)

def setup_database():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute('''CREATE TABLE IF NOT EXISTS champions (id TEXT PRIMARY KEY, name TEXT, cost INTEGER, traits TEXT, image_path TEXT)''')
    cursor.execute('''CREATE TABLE IF NOT EXISTS items (id INTEGER PRIMARY KEY, name TEXT, is_base_item BOOLEAN, image_path TEXT)''')
    conn.commit()
    return conn

def download_image(cd_path, save_folder, file_name):
    if not cd_path: return ""
    clean_path = cd_path.lower().replace(".tex", ".png")
    url = f"{IMAGE_BASE_URL}{clean_path}"
    save_path = ASSETS_DIR / save_folder / f"{file_name}.png"
    if not save_path.exists():
        resp = requests.get(url)
        if resp.status_code == 200:
            with open(save_path, 'wb') as f:
                f.write(resp.content)
    return str(save_path)

def pipeline_run():
    print("Đang cào dữ liệu từ CDragon...")
    data = requests.get(BASE_URL).json()
    conn = setup_database()
    cursor = conn.cursor()
    
    # 1. XỬ LÝ TRANG BỊ (ITEMS)
    print("Đang xử lý Trang bị...")
    for item in data.get('items', []):
        # NÂNG CẤP LỌC RÁC: Kiểm tra thêm id phải là số nguyên (int)
        if not item.get('name') or not item.get('icon') or not isinstance(item.get('id'), int):
            continue
            
        item_id = item['id']
        name = item['name']
        
        # Lúc này item_id chắc chắn là số, phép so sánh sẽ không bao giờ bị lỗi
        is_base = 1 <= item_id <= 9 
        
        # Tải icon
        img_path = download_image(item['icon'], "items", f"item_{item_id}")
        
        cursor.execute('''
            INSERT OR REPLACE INTO items (id, name, is_base_item, image_path)
            VALUES (?, ?, ?, ?)
        ''', (item_id, name, is_base, img_path))

    latest_set_id = list(data['sets'].keys())[-1]
    for champ in data['sets'][latest_set_id].get('champions', []):
        if not champ.get('traits') or champ.get('cost') == 0: continue
        champ_id, name, cost = champ['apiName'], champ['name'], champ['cost']
        traits = json.dumps(champ['traits'], ensure_ascii=False)
        img_path = download_image(champ['icon'], "champions", champ_id)
        cursor.execute('INSERT OR REPLACE INTO champions VALUES (?, ?, ?, ?, ?)', (champ_id, name, cost, traits, img_path))

    conn.commit()
    conn.close()
    print("Hoàn tất Data Pipeline!")

if __name__ == "__main__":
    pipeline_run()