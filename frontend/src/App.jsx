import React, { useState, useEffect } from 'react';
import axios from 'axios';
import useTftStore from './store/useTftStore';

const BASIC_ITEMS = [
  { id: 1, name: "Kiếm BF", icon: "⚔️" },
  { id: 2, name: "Cung Gỗ", icon: "🏹" },
  { id: 3, name: "Gậy Quá", icon: "🪄" },
  { id: 4, name: "Nước Mắt", icon: "💧" },
  { id: 5, name: "Giáp Lưới", icon: "🛡️" },
  { id: 6, name: "Áo Choàng", icon: "🧥" },
  { id: 7, name: "Đai Khổng", icon: "🥋" },
  { id: 8, name: "Găng Tập", icon: "🥊" },
  { id: 9, name: "Xẻng Vàng", icon: "🍳" }
];

function App() {
  const [searchTerm, setSearchTerm] = useState("");

  const { 
    championsDb, fetchChampions, 
    level, setLevel, 
    teamIds, addChampion, removeChampion,
    inventory, updateItemCount, 
    boardConfigs, updateBoardConfig, // <-- Lấy state quản lý Role & Slot từ V2
    isOptimizing, setOptimizing, 
    resultData, setResultData,
    getOptimizationPayload 
  } = useTftStore();

  useEffect(() => {
    fetchChampions();
  }, []);

  const filteredChamps = championsDb.filter(champ => 
    champ.name.toLowerCase().includes(searchTerm.toLowerCase())
  );

  const handleOptimize = async () => {
    setOptimizing(true);
    setResultData(null); 

    try {
      const payload = getOptimizationPayload();
      const response = await axios.post('http://127.0.0.1:8000/api/v1/optimize', payload);
      
      if (response.data.status === 'success') {
        setResultData(response.data.data);
      }
    } catch (error) {
      console.error("Lỗi khi kết nối Server:", error);
      alert("Không thể kết nối đến Máy chủ Tối ưu! Hãy kiểm tra xem uvicorn (FastAPI) đã được bật chưa.");
    } finally {
      setOptimizing(false);
    }
  };

  return (
    <div className="min-h-screen p-4 md:p-8 font-sans flex flex-col bg-slate-950 text-white">
      <header className="mb-8 border-b border-slate-700 pb-4">
        <h1 className="text-3xl font-bold text-transparent bg-clip-text bg-gradient-to-r from-blue-400 to-purple-500">
          TFT Optimizer
        </h1>
        <p className="text-slate-400 mt-2">Hệ thống tối ưu đội hình</p>
      </header>

      <main className="grid grid-cols-1 lg:grid-cols-2 gap-8 mb-8">
        {/* ================= CỘT TRÁI: CẤU HÌNH ĐỘI HÌNH ================= */}
        <section className="bg-slate-800 p-6 rounded-xl shadow-lg border border-slate-700 flex flex-col h-[650px]">
          <h2 className="text-xl font-semibold mb-4 text-blue-300">1. Đội hình & Vai trò</h2>
          
          <div className="flex items-center gap-4 mb-4">
            <label className="text-slate-300 font-medium">Level hiện tại:</label>
            <select 
              value={level} 
              onChange={(e) => setLevel(Number(e.target.value))}
              className="bg-slate-900 border border-slate-600 rounded px-3 py-1.5 text-white focus:outline-none focus:border-blue-500"
            >
              {[...Array(10).keys()].map(i => (
                <option key={i+1} value={i+1}>Cấp {i+1}</option>
              ))}
            </select>
          </div>

          {/* GIAO DIỆN CHỌN VAI TRÒ MỚI CỦA V2 */}
          <div className="mb-4">
            <h3 className="text-sm text-slate-400 font-medium mb-2">Bàn cờ ({teamIds.length}/{level}):</h3>
            <div className="grid grid-cols-2 md:grid-cols-3 gap-2 min-h-[120px] p-3 border border-slate-600 rounded-lg bg-slate-900/80 shadow-inner overflow-y-auto">
              {teamIds.length === 0 && <span className="text-slate-500 text-sm italic col-span-full text-center mt-8">Bấm vào danh sách dưới để thêm tướng</span>}
              
              {teamIds.map(champId => {
                const champ = championsDb.find(c => c.id === champId) || { name: champId };
                const config = boardConfigs[champId] || { role: "utility", slotsLeft: 3 };

                return (
                  <div key={champId} className="relative p-2 border border-slate-600 rounded-lg bg-slate-800 text-white flex flex-col items-center shadow-md">
                    {/* Nút Xóa Tướng */}
                    <button 
                      onClick={() => removeChampion(champId)}
                      className="absolute top-0 right-1 text-slate-400 hover:text-red-400 font-bold text-lg"
                    >×</button>
                    
                    {/* Tên Tướng */}
                    <span className="font-bold text-sm mb-2 text-blue-200 truncate w-full text-center">{champ.name}</span>
                    
                    {/* 1. BẢNG CHỌN VAI TRÒ (Đoạn code bạn vừa hỏi) */}
                    <select 
                      className={`w-full text-xs p-1 font-semibold border rounded mb-1 focus:outline-none ${
                        config.role === 'ap_carry' ? 'bg-purple-900/50 border-purple-500 text-purple-200' :
                        config.role === 'ad_carry' ? 'bg-orange-900/50 border-orange-500 text-orange-200' :
                        config.role === 'tank' ? 'bg-green-900/50 border-green-500 text-green-200' :
                        'bg-slate-700 border-slate-500 text-slate-300'
                      }`}
                      value={config.role}
                      onChange={(e) => updateBoardConfig(champId, "role", e.target.value)}
                    >
                      <option value="utility">Thường</option>
                      <option value="ap_carry">Carry Phép 🪄</option>
                      <option value="ad_carry">Carry Vật Lý ⚔️</option>
                      <option value="tank">Tank 🛡️</option>
                    </select>

                    {/* 2. MENU ĐẶC TÍNH (Chỉ hiện khi chọn Carry Phép) */}
                    {config.role === "ap_carry" && (
                      <select 
                        className="w-full text-[10px] p-1 mt-1 bg-slate-900 border border-purple-500/50 text-purple-300 rounded focus:outline-none"
                        value={config.modifier || "none"}
                        onChange={(e) => updateBoardConfig(champId, "modifier", e.target.value)}
                      >
                        <option value="none">-- Tình trạng Tướng --</option>
                        <option value="has_crit">Đã có Chí Mạng</option>
                        <option value="high_ap">Hệ cho nhiều AP</option>
                        <option value="time_scale">Scale theo thời gian</option>
                      </select>
                    )}
                    {/* MENU ĐẶC TÍNH (Dành cho Carry Vật Lý) */}
                    {config.role === "ad_carry" && (
                      <select 
                        className="w-full text-[10px] p-1 mt-1 bg-slate-900 border border-orange-500/50 text-orange-300 rounded focus:outline-none"
                        value={config.modifier || "none"}
                        onChange={(e) => updateBoardConfig(champId, "modifier", e.target.value)}
                      >
                        <option value="none">-- Tình trạng Vật Lý --</option>
                        <option value="high_as">Nhiều Tốc Đánh (Cần STVL)</option>
                        <option value="need_arp">Cần Xuyên Giáp</option>
                        <option value="physical_caster">Phụ thuộc Chiêu (Sát lực)</option>
                        <option value="balanced_mix">Công Thủ Toàn Diện (Balanced)</option>
                      </select>
                    )}
                    {/* 3. Ô CHỌN SLOT ĐỒ (Ẩn đi nếu là tướng Thường) */}
                    {config.role !== "utility" && (
                      <div className="flex items-center w-full justify-between mt-1 text-xs text-slate-300 bg-slate-900 px-1 py-1 rounded">
                        <span>Slot đồ:</span>
                        <input 
                          type="number" min="1" max="3" 
                          value={config.slotsLeft}
                          onChange={(e) => updateBoardConfig(champId, "slotsLeft", parseInt(e.target.value))}
                          className="w-8 bg-transparent text-center font-bold outline-none"
                        />
                      </div>
                    )}
                  </div>
                );
              })}
            </div>
          </div>

          <div className="flex flex-col flex-grow overflow-hidden">
            <input 
              type="text" 
              placeholder="🔍 Tìm tướng..." 
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              className="w-full bg-slate-900 border border-slate-600 rounded px-3 py-2 text-white mb-3 focus:border-blue-500 outline-none"
            />
            <div className="overflow-y-auto pr-2 grid grid-cols-2 gap-2 flex-grow">
              {filteredChamps.map(champ => {
                const isSelected = teamIds.includes(champ.id);
                const isBoardFull = teamIds.length >= level;
                return (
                  <button
                    key={champ.id}
                    onClick={() => addChampion(champ.id)}
                    disabled={isSelected || isBoardFull}
                    className={`flex items-center justify-between p-2 rounded border text-left transition-all ${
                      isSelected ? 'bg-blue-900/40 border-blue-500 text-blue-300 opacity-50' : 'bg-slate-700 border-slate-600 hover:border-blue-400 text-slate-200'
                    }`}
                  >
                    <span className="font-medium text-sm">{champ.name}</span>
                    <span className="text-xs font-bold text-yellow-500">${champ.cost}</span>
                  </button>
                )
              })}
            </div>
          </div>
        </section>

        {/* ================= CỘT PHẢI: KHO ĐỒ ================= */}
        <section className="bg-slate-800 p-6 rounded-xl shadow-lg border border-slate-700 flex flex-col h-[650px]">
          <h2 className="text-xl font-semibold mb-4 text-purple-300">2. Kho Đồ Khởi Điểm</h2>
          
          <div className="grid grid-cols-3 gap-3 mb-6 flex-grow">
            {BASIC_ITEMS.map((item, index) => (
              <div key={item.id} className={`flex flex-col items-center p-3 rounded-lg border transition-colors ${inventory[index] > 0 ? 'bg-slate-700 border-purple-500 shadow-[0_0_10px_rgba(168,85,247,0.15)]' : 'bg-slate-900/50 border-slate-700'}`}>
                <span className="text-3xl mb-2">{item.icon}</span>
                <span className="text-xs text-center text-slate-300 font-medium mb-3">{item.name}</span>
                <div className="flex items-center gap-2 mt-auto bg-slate-800 rounded-md p-1 border border-slate-600 w-full justify-between">
                  <button onClick={() => updateItemCount(index, -1)} disabled={inventory[index] === 0} className="w-7 h-7 flex items-center justify-center rounded bg-slate-700 text-white disabled:opacity-30 hover:bg-red-500 transition-colors">-</button>
                  <span className="font-bold text-sm text-center text-blue-300">{inventory[index]}</span>
                  <button onClick={() => updateItemCount(index, 1)} className="w-7 h-7 flex items-center justify-center rounded bg-slate-700 text-white hover:bg-green-500 transition-colors">+</button>
                </div>
              </div>
            ))}
          </div>

          <button 
            onClick={handleOptimize}
            disabled={isOptimizing}
            className={`w-full py-4 rounded-lg font-bold text-lg transition-all shadow-lg active:scale-[0.98] ${
              isOptimizing 
                ? 'bg-slate-700 cursor-not-allowed text-slate-500 border border-slate-600' 
                : 'bg-gradient-to-r from-blue-600 to-purple-600 hover:from-blue-500 hover:to-purple-500 text-white shadow-purple-500/25 border border-purple-400/50'
            }`}
          >
            {isOptimizing ? "🤖 ĐANG TÍNH TOÁN..." : "🚀 TỐI ƯU HÓA ĐỘI HÌNH & TRANG BỊ"}
          </button>
        </section>
      </main>

      {/* ================= KHU VỰC KẾT QUẢ V2 ================= */}
      <section className="bg-slate-800 p-6 rounded-xl shadow-lg border border-green-500/30 flex-grow">
        <h2 className="text-xl font-semibold mb-4 text-green-400 flex items-center gap-2">
          <span>✨</span> Đội Hình Tối Ưu
        </h2>
        
        {!resultData ? (
          <div className="p-8 border border-slate-600 border-dashed rounded-lg bg-slate-900/50 flex flex-col items-center justify-center text-slate-500">
             <span className="text-4xl mb-3 opacity-50">🤖</span>
             <p>Cấu hình tướng, cấp độ và kho đồ ở trên, sau đó bấm nút Tối Ưu Hóa.</p>
          </div>
        ) : (
          <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
            
            {/* Kết quả Đội hình */}
            <div className="bg-slate-900 p-5 rounded-lg border border-slate-700">
              <div className="flex justify-between items-center mb-4 pb-2 border-b border-slate-700">
                <h3 className="font-semibold text-blue-300">Tướng Đề Xuất Thêm</h3>
                <span className="bg-blue-900/50 text-blue-400 px-3 py-1 rounded-full text-sm font-bold border border-blue-500/30">
                  Điểm Kích Hệ: {resultData.max_synergy_score}
                </span>
              </div>
              <div className="flex flex-wrap gap-2">
                {resultData.recommended_team.length > 0 ? (
                  resultData.recommended_team.map(id => {
                    const champ = championsDb.find(c => c.id === id) || { name: id };
                    return (
                      <span key={id} className="bg-blue-950 text-blue-200 border border-blue-700/50 px-3 py-1.5 rounded-md text-sm font-medium shadow-sm">
                        + {champ.name}
                      </span>
                    )
                  })
                ) : (
                  <span className="text-slate-500 text-sm italic">Bàn cờ đã đầy hoặc không tìm được tướng phù hợp.</span>
                )}
              </div>
            </div>

            {/* Kết quả Trang bị V2 (Hỗ trợ Đa Role) */}
            <div className="bg-slate-900 p-5 rounded-lg border border-slate-700">
              <div className="flex justify-between items-center mb-4 pb-2 border-b border-slate-700">
                <h3 className="font-semibold text-purple-300">Phân Bổ Trang Bị</h3>
                <span className="bg-purple-900/50 text-purple-400 px-3 py-1 rounded-full text-sm font-bold border border-purple-500/30">
                  Sức Mạnh Đồ: {resultData.max_item_power}
                </span>
              </div>
              
              <div className="flex flex-col gap-3">
                {resultData.item_assignments && resultData.item_assignments.length > 0 ? (
                  resultData.item_assignments.map((assignment, idx) => {
                    const champName = championsDb.find(c => c.id === assignment.champ_id)?.name || assignment.champ_id;
                    return (
                      <div key={idx} className="bg-purple-950/40 border border-purple-800/50 p-3 rounded-lg">
                        <div className="flex justify-between items-center mb-2">
                          <span className="font-bold text-purple-300">👉 {champName}</span>
                          <span className="text-xs bg-purple-900 text-purple-200 px-2 py-1 rounded border border-purple-700">Điểm: {assignment.power_score}</span>
                        </div>
                        <div className="space-y-1.5">
                          {assignment.item_names.map((itemStr, i) => (
                            <div key={i} className="text-sm text-purple-100 flex items-center gap-2 bg-slate-900/50 px-2 py-1 rounded">
                              <span>🛠️</span> {itemStr}
                            </div>
                          ))}
                        </div>
                      </div>
                    )
                  })
                ) : (
                  <span className="text-slate-500 text-sm italic">Kho đồ trống hoặc không có đồ phù hợp cho Carry/Tank.</span>
                )}
              </div>
            </div>

          </div>
        )}
      </section>
    </div>
  );
}

export default App;