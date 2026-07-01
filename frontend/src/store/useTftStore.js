import { create } from 'zustand';
import axios from 'axios';

const useTftStore = create((set, get) => ({
  championsDb: [], 
  level: 8,
  teamIds: [], 
  inventory: [0, 0, 0, 0, 0, 0, 0, 0, 0],
  boardConfigs: {}, 
  isOptimizing: false,
  resultData: null,

  fetchChampions: async () => {
    try {
      const response = await axios.get('http://127.0.0.1:8000/api/v1/champions');
      if (response.data.status === 'success') {
        set({ championsDb: response.data.data });
      }
    } catch (error) {
      console.error("Lỗi khi tải danh sách tướng:", error);
    }
  },

  setLevel: (newLevel) => set({ level: newLevel }),
  
  addChampion: (champId) => set((state) => {
    if (state.teamIds.includes(champId) || state.teamIds.length >= 10) return state;
    return { 
      teamIds: [...state.teamIds, champId],
      boardConfigs: {
        ...state.boardConfigs,
        [champId]: { role: "utility", modifier: "none", slotsLeft: 3 }
      }
    };
  }),
  
  removeChampion: (champId) => set((state) => {
    const newConfigs = { ...state.boardConfigs };
    delete newConfigs[champId];
    return {
      teamIds: state.teamIds.filter(id => id !== champId),
      boardConfigs: newConfigs
    };
  }),
  
  updateItemCount: (index, change) => set((state) => {
    const newInv = [...state.inventory];
    newInv[index] = Math.max(0, newInv[index] + change); 
    return { inventory: newInv };
  }),
  
  updateBoardConfig: (champId, field, value) => set((state) => ({
    boardConfigs: {
      ...state.boardConfigs,
      [champId]: {
        ...state.boardConfigs[champId],
        [field]: value
      }
    }
  })),

  // === ĐÃ SỬA: MỞ KHÓA CHO AP_CARRY VÀ AD_CARRY ===
  getOptimizationPayload: () => {
    const state = get();
    
    const configsArray = Object.keys(state.boardConfigs)
      .filter(id => ["carry", "ap_carry", "ad_carry", "tank"].includes(state.boardConfigs[id].role))
      .map(id => ({
        id: id,
        role: state.boardConfigs[id].role,
        modifier: state.boardConfigs[id].modifier || "none", // Bơm đặc tính xuống Python
        slots_left: state.boardConfigs[id].slotsLeft
      }));

    return {
      level: state.level,
      team_ids: state.teamIds,
      inventory: state.inventory,
      board_configs: configsArray 
    };
  },

  setResultData: (data) => set({ resultData: data }),
  setOptimizing: (status) => set({ isOptimizing: status })
}));

export default useTftStore;