/**
 * Demo Player 前端 API 模块
 * 封装 Axios 请求与 Token 自动拦截、刷新
 */

const API_BASE = '';

const apiClient = axios.create({
  baseURL: API_BASE,
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json'
  }
});

// 请求拦截器：注入 Token
apiClient.interceptors.request.use(
  config => {
    const token = localStorage.getItem('demo_player_token');
    if (token) {
      config.headers['Authorization'] = `Bearer ${token}`;
    }
    return config;
  },
  error => Promise.reject(error)
);

// 响应拦截器：处理 401 自动刷新 Token
apiClient.interceptors.response.use(
  response => response.data,
  async error => {
    const originalRequest = error.config;
    if (error.response && error.response.status === 401 && !originalRequest._retry) {
      originalRequest._retry = true;
      const refreshToken = localStorage.getItem('demo_player_refresh_token');
      if (refreshToken) {
        try {
          const res = await axios.post('/api/auth/refresh-token', { refresh_token: refreshToken });
          if (res.data && res.data.code === 200 && res.data.data.token) {
            localStorage.setItem('demo_player_token', res.data.data.token);
            originalRequest.headers['Authorization'] = `Bearer ${res.data.data.token}`;
            return axios(originalRequest);
          }
        } catch (e) {
          localStorage.removeItem('demo_player_token');
          localStorage.removeItem('demo_player_refresh_token');
          localStorage.removeItem('demo_player_user');
        }
      }
    }
    return Promise.reject(error.response ? error.response.data : error);
  }
);

window.API = {
  // 认证
  sendCode: (target, type = 'register') => apiClient.post('/api/auth/send-code', { target, type }),
  verifyCode: (target, code) => apiClient.post('/api/auth/verify-code', { target, code }),
  register: (data) => apiClient.post('/api/auth/register', data),
  login: (account, password) => apiClient.post('/api/auth/login', { account, password }),
  refreshToken: (refreshToken) => apiClient.post('/api/auth/refresh-token', { refresh_token: refreshToken }),
  resetPassword: (data) => apiClient.post('/api/auth/reset-password', data),
  getUserProfile: () => apiClient.get('/api/user/profile'),

  // 搜索与音乐 (后端自动剔除不可播放歌曲)
  searchSuggest: (keywords) => apiClient.get(`/api/search/suggest?keywords=${encodeURIComponent(keywords)}`),
  searchSongs: (keywords, limit = 30, offset = 0) => apiClient.get(`/api/song/search?keywords=${encodeURIComponent(keywords)}&limit=${limit}&offset=${offset}`),
  getSongUrl: (id, level = 'standard') => apiClient.get(`/api/song/url?id=${id}&level=${level}`),
  getSongLyric: (id) => apiClient.get(`/api/song/lyric?id=${id}`),

  // 歌单与分类
  getPlaylistCatlist: () => apiClient.get('/api/playlist/catlist'),
  getHotPlaylists: (limit = 20, offset = 0, cat = '全部') => apiClient.get(`/api/playlist/hot?limit=${limit}&offset=${offset}&cat=${encodeURIComponent(cat)}`),
  getPlaylistDetail: (id) => apiClient.get(`/api/playlist/detail?id=${id}`),

  // 用户歌单
  getUserPlaylists: () => apiClient.get('/api/user/playlists'),
  createUserPlaylist: (data) => apiClient.post('/api/user/playlists/create', data),
  getPlaylistTracks: (playlistId) => apiClient.get(`/api/user/playlists/tracks?playlist_id=${playlistId}`),

  // 收藏与历史
  toggleFavorite: (song) => apiClient.post('/api/favorite/toggle', song),
  getFavorites: () => apiClient.get('/api/favorite/list'),
  checkFavorite: (songId) => apiClient.get(`/api/favorite/check?song_id=${songId}`),
  recordHistory: (song) => apiClient.post('/api/history/record', song),
  getHistory: () => apiClient.get('/api/history/list'),

  // 🤖 AI Agent 智能推荐
  agentRecommend: (data) => apiClient.post('/api/agent/recommend', data),
  testLLM: (data) => apiClient.post('/api/agent/test-llm', data)
};
