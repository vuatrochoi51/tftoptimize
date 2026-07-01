import requests
import time

API_URL = "http://127.0.0.1:8000/api/v1/optimize"

# =====================================================================
# KỊCH BẢN TEST V2.2: ÉP HỆ DỌC VÀ ĐIỂM TIỀM NĂNG (DELTA BONUS)
# =====================================================================
payload = {
    "level": 8, 
    # Bàn cờ mới có 3 tướng, còn tận 5 slot trống.
    # Ở Level 8, tỉ lệ ra tướng 4-5 vàng rất cao -> Cám dỗ sức mạnh rất lớn.
    "team_ids": ["TFT17_Akali", "TFT17_Maokai", "TFT17_Shen"], 
    
    "inventory": [1, 2, 0, 0, 0, 0, 0, 0, 0], # Đồ cơ bản, không quan trọng trong test này
    
    "board_configs": [
        {
            "id": "TFT17_Akali",
            "role": "carry", # KHAI BÁO CARRY ĐỂ AI KÍCH HOẠT MULTIPLIER (x1.5 -> x3.0)
            "slots_left": 3 
        },
        {
            "id": "TFT17_Maokai",
            "role": "tank",
            "slots_left": 3 
        }
    ]
}

def run_test():
    print(f"🚀 Đang gửi Kịch bản Test 'Cám Dỗ Level 8' lên {API_URL}...")
    start_time = time.time()

    try:
        response = requests.post(API_URL, json=payload)
        response.raise_for_status() 
        
        time_ms = (time.time() - start_time) * 1000
        data = response.json()
        
        print(f"\n✅ BUM! Phản hồi sau: {time_ms:.2f} mili-giây")
        print("="*60)
        print("🎯 KẾT QUẢ BÀI TEST ÉP HỆ DỌC (VERTICAL TRAIT):")
        
        result_data = data.get("data", {})
        
        recommended = result_data.get('recommended_team', [])
        synergy_score = result_data.get('max_synergy_score', 0)
        
        print(f"\n   - Điểm kích hệ (Đã nhân hệ số lũy thừa): {synergy_score} điểm")
        print(f"   - 5 Tướng được AI chọn thêm:\n     {recommended}")
        
        print("\n   -> [Kỳ Vọng]: AI sẽ KHÔNG chọn ngẫu nhiên các tướng 5 Vàng rác (Exodia).")
        print("                 Thay vào đó, nó sẽ gọi ra những tướng rẻ tiền hơn nhưng ")
        print("                 có chung Tộc/Hệ với Akali để đẩy mốc kích hệ lên tối đa!")
        print("="*60)
        
    except Exception as e:
        print(f"\n❌ LỖI KHÔNG XÁC ĐỊNH: {e}")

if __name__ == "__main__":
    run_test()