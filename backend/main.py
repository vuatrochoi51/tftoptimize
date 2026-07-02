from contextlib import asynccontextmanager
import sqlite3
import json
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import List
import tft_core_engine 

# Danh sách các từ khóa Tộc/Hệ thuộc nhóm Tanker (Chịu đòn)
TANK_TRAITS = ["Đấu Sĩ", "Hộ Vệ", "Tiên Phong", "Can Trường", "Vệ Sĩ", "Khổng Lồ", "Hóa Hình", "Bruiser", "Vanguard", "Bastion", "Defender"]

@asynccontextmanager
async def lifespan(app: FastAPI):
    print("Khởi động Server và Nạp C++ Engine...")
    try:
        conn = sqlite3.connect('tft_meta.db')
        cursor = conn.cursor()
        
        cursor.execute("SELECT id, traits, cost FROM champions")
        champion_rows = cursor.fetchall()
        
        all_champs = []
        all_costs = [] 
        all_tanks = [] 
        trait_dict = {} 
        
        for row in champion_rows:
            champ_id = row[0]
            traits_of_champ = json.loads(row[1])
            cost = int(row[2]) 
            
            all_champs.append(champ_id)
            all_costs.append(cost) 
            
            is_tank = any(t_name in TANK_TRAITS for t_name in traits_of_champ)
            all_tanks.append(is_tank) 
            
            for t_name in traits_of_champ:
                if t_name not in trait_dict:
                    trait_dict[t_name] = []
                trait_dict[t_name].append(champ_id)
                
        list_traits = []
        for trait_name, champ_list in trait_dict.items():
            t_data = tft_core_engine.TraitData()
            t_data.name = trait_name
            t_data.breakpoints = [2, 4, 6, 8]
            t_data.scores = [15, 35, 60, 100] 
            t_data.champion_ids = champ_list
            list_traits.append(t_data)
        
        tft_core_engine.init_engine(all_champs, all_costs, all_tanks, list_traits)
        conn.close()
    except Exception as e:
        print(f"Lỗi khởi tạo hệ thống lõi: {e}")
    yield
    print("Tắt Server...")

app = FastAPI(lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"], 
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

class BoardConfig(BaseModel):
    id: str
    role: str
    modifier: str = "none" 
    slots_left: int

class OptimizeRequest(BaseModel):
    level: int
    team_ids: List[str]
    inventory: List[int]
    board_configs: List[BoardConfig]

@app.get("/api/v1/champions")
async def get_champions():
    try:
        conn = sqlite3.connect('tft_meta.db')
        cursor = conn.cursor()
        cursor.execute("SELECT id, name, traits, cost, image_path FROM champions")
        rows = cursor.fetchall()
        conn.close()
        
        champs = []
        for r in rows:
            champs.append({
                "id": r[0],
                "name": r[1],
                "traits": json.loads(r[2]),
                "cost": int(r[3]),
                "image_path": r[4]
            })
            
        return {"status": "success", "data": champs}
    except Exception as e:
        return {"status": "error", "message": str(e)}

@app.post("/api/v1/optimize")
async def optimize_team(request: OptimizeRequest):
    try:
        board_configs = []
        for c in request.board_configs:
            conf = tft_core_engine.ChampionConfig()
            conf.id = c.id
            conf.role = c.role
            conf.modifier = c.modifier # Bơm modifier xuống C++
            conf.slots_left = c.slots_left
            board_configs.append(conf)
            
        result = tft_core_engine.run_optimization(
            request.level,
            request.team_ids,
            request.inventory,
            board_configs
        )
        
        best_items_list = []
        for assign in result.item_assignments:
            best_items_list.append({
                "champ_id": assign.champ_id,
                "power_score": assign.power_score,
                "item_names": assign.item_names
            })
            
        return {
            "status": "success", 
            "data": {
                "recommended_team": result.recommended_team,
                "max_synergy_score": result.max_synergy_score,
                "max_item_power": result.max_item_power,
                "item_assignments": best_items_list
            }
        }
    except Exception as e:
        return {"status": "error", "message": str(e)}