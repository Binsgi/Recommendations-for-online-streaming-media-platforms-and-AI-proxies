/**
 * Demo Player - Vue.js Core Application
 * 现代化全功能音乐播放器与 AI Agent 智能推荐工作台
 */

new Vue({
  el: '#app',
  data() {
    return {
      // 导航与视窗
      activeTab: 'search', // 'search' | 'playlist' | 'favorite' | 'history' | 'agent' | 'vinyl'

      // 搜索与联想
      query: '周杰伦',
      searchSuggestions: [],
      showSuggestions: false,
      musicList: [],
      isSearching: false,

      // 歌单与分类
      categories: ['全部', '华语', '欧美', '流行', '摇滚', '民谣', '电子', '轻音乐', '治愈'],
      currentCategory: '全部',
      hotPlaylists: [],
      currentPlaylistDetail: null,

      // 用户认证
      currentUser: null,
      showAuthModal: false,
      authMode: 'login', // 'login' | 'register' | 'forgot'
      authForm: {
        account: '',
        password: '',
        phone_email: '',
        code: '',
        nickname: '',
        new_password: ''
      },
      codeCountdown: 0,
      codeTimer: null,

      // 收藏与历史
      favoritesList: [],
      historyList: [],
      isCurrentSongFav: false,

      // 🤖 AI Agent 智能推荐
      agentPrompt: '深夜写代码，来几首安静专注的Lo-Fi或轻音乐',
      agentMood: '专注',
      agentGenre: '轻音乐',
      agentLoading: false,
      agentReply: null,
      agentPresetChips: [
        '💻 深夜写代码专注',
        '🌙 治愈安眠白噪音',
        '⚡ 燃向运动健身',
        '🌧️ 伤感emo走心慢歌',
        '🎸 周杰伦时代金曲',
        '🍵 唯美国风新潮'
      ],

      // 播放器核心状态
      currentSong: {
        id: '',
        name: '未播放歌曲',
        artist: '暂无歌手',
        album: '',
        cover: 'images/cover.png',
        url: '',
        duration: 0
      },
      playQueue: [],
      currentQueueIndex: -1,
      isPlay: false,
      currentTime: 0,
      totalDuration: 0,
      progressPercent: 0,
      volume: 0.8,
      soundQuality: 'standard',
      playMode: 'order', // 'order' | 'single' | 'random'

      // 歌词系统
      rawLyrics: [],
      currentLyricIndex: 0,
      animationFrameId: null,

      // MV 播放
      mvUrl: '',
      showMv: false,

      // Toast 消息
      toastMessage: '',
      toastTimer: null
    };
  },

  computed: {
    formatCurrentTime() {
      return this.formatSeconds(this.currentTime);
    },
    formatTotalDuration() {
      return this.formatSeconds(this.totalDuration);
    }
  },

  created() {
    this.checkLocalAuth();
    this.searchMusic();
    this.loadHotPlaylists();
  },

  mounted() {
    this.initAudioEvents();
    // 全局点击关闭建议下拉框
    document.addEventListener('click', (e) => {
      if (!this.$refs.searchGroup || !this.$refs.searchGroup.contains(e.target)) {
        this.showSuggestions = false;
      }
    });
  },

  methods: {
    showToast(msg) {
      this.toastMessage = msg;
      if (this.toastTimer) clearTimeout(this.toastTimer);
      this.toastTimer = setTimeout(() => {
        this.toastMessage = '';
      }, 3000);
    },

    formatSeconds(seconds) {
      if (!seconds || isNaN(seconds)) return '00:00';
      const m = Math.floor(seconds / 60);
      const s = Math.floor(seconds % 60);
      return `${m < 10 ? '0' + m : m}:${s < 10 ? '0' + s : s}`;
    },

    // ==================== 认证相关 ====================
    checkLocalAuth() {
      const userStr = localStorage.getItem('demo_player_user');
      if (userStr) {
        try {
          this.currentUser = JSON.parse(userStr);
          this.loadFavorites();
          this.loadHistory();
        } catch (e) {
          localStorage.removeItem('demo_player_user');
        }
      }
    },

    openAuth(mode = 'login') {
      this.authMode = mode;
      this.showAuthModal = true;
    },

    logout() {
      this.currentUser = null;
      localStorage.removeItem('demo_player_token');
      localStorage.removeItem('demo_player_refresh_token');
      localStorage.removeItem('demo_player_user');
      this.favoritesList = [];
      this.historyList = [];
      this.isCurrentSongFav = false;
      this.showToast('已安全退出登录');
    },

    async handleSendCode() {
      const target = this.authForm.phone_email || this.authForm.account;
      if (!target) {
        this.showToast('请先输入手机号或邮箱');
        return;
      }

      try {
        const res = await API.sendCode(target, this.authMode);
        if (res.code === 200) {
          this.showToast(`验证码已发送！${res.data.debug_code ? ' [调试码: ' + res.data.debug_code + ']' : ''}`);
          this.codeCountdown = 60;
          this.codeTimer = setInterval(() => {
            this.codeCountdown--;
            if (this.codeCountdown <= 0) {
              clearInterval(this.codeTimer);
            }
          }, 1000);
        } else {
          this.showToast(res.message || '发送失败');
        }
      } catch (err) {
        this.showToast('发送验证码失败');
      }
    },

    async handleAuthSubmit() {
      try {
        if (this.authMode === 'login') {
          if (!this.authForm.account || !this.authForm.password) {
            this.showToast('请输入账号和密码');
            return;
          }
          const res = await API.login(this.authForm.account, this.authForm.password);
          if (res.code === 200) {
            this.currentUser = res.data.user;
            localStorage.setItem('demo_player_token', res.data.token);
            localStorage.setItem('demo_player_refresh_token', res.data.refresh_token);
            localStorage.setItem('demo_player_user', JSON.stringify(res.data.user));
            this.showAuthModal = false;
            this.showToast('欢迎回来，' + (this.currentUser.nickname || this.currentUser.username));
            this.loadFavorites();
            this.loadHistory();
          } else {
            this.showToast(res.message || '登录失败');
          }
        } else if (this.authMode === 'register') {
          if (!this.authForm.account || !this.authForm.password) {
            this.showToast('用户名和密码不能为空');
            return;
          }
          const res = await API.register({
            username: this.authForm.account,
            password: this.authForm.password,
            phone_email: this.authForm.phone_email,
            code: this.authForm.code,
            nickname: this.authForm.nickname
          });
          if (res.code === 200) {
            this.showToast('注册成功！正在为您自动登录...');
            this.handleAuthSubmitLoginDirect();
          } else {
            this.showToast(res.message || '注册失败');
          }
        } else if (this.authMode === 'forgot') {
          const res = await API.resetPassword({
            target: this.authForm.phone_email,
            code: this.authForm.code,
            new_password: this.authForm.new_password
          });
          if (res.code === 200) {
            this.showToast('密码修改成功，请使用新密码登录');
            this.authMode = 'login';
          } else {
            this.showToast(res.message || '重置密码失败');
          }
        }
      } catch (err) {
        this.showToast(err.message || '操作失败');
      }
    },

    async handleAuthSubmitLoginDirect() {
      const res = await API.login(this.authForm.account, this.authForm.password);
      if (res.code === 200) {
        this.currentUser = res.data.user;
        localStorage.setItem('demo_player_token', res.data.token);
        localStorage.setItem('demo_player_refresh_token', res.data.refresh_token);
        localStorage.setItem('demo_player_user', JSON.stringify(res.data.user));
        this.showAuthModal = false;
        this.loadFavorites();
      }
    },

    // ==================== 搜索与联想 ====================
    async onSearchInput() {
      if (!this.query.trim()) {
        this.searchSuggestions = [];
        this.showSuggestions = false;
        return;
      }
      try {
        const res = await API.searchSuggest(this.query.trim());
        if (res.code === 200 && res.result && res.result.allMatch) {
          this.searchSuggestions = res.result.allMatch;
          this.showSuggestions = true;
        }
      } catch (e) {
        this.searchSuggestions = [];
      }
    },

    selectSuggestion(keyword) {
      this.query = keyword;
      this.showSuggestions = false;
      this.searchMusic();
    },

    async searchMusic() {
      if (!this.query.trim()) return;
      this.showSuggestions = false;
      this.isSearching = true;
      try {
        // 后端自动剔除不可播放/url为空的歌曲
        const res = await API.searchSongs(this.query.trim(), 30);
        if (res.code === 200 && res.songs) {
          this.musicList = res.songs;
          if (this.musicList.length === 0) {
            this.showToast('未找到可播放的歌曲');
          }
        }
      } catch (err) {
        this.showToast('搜索失败，请检查网络或后端');
      } finally {
        this.isSearching = false;
      }
    },

    // ==================== 歌单分类与热门 ====================
    async selectCategory(cat) {
      this.currentCategory = cat;
      this.loadHotPlaylists();
    },

    async loadHotPlaylists() {
      try {
        const res = await API.getHotPlaylists(18, 0, this.currentCategory);
        if (res.playlists) {
          this.hotPlaylists = res.playlists;
        }
      } catch (e) {}
    },

    async openPlaylistDetail(playlist) {
      try {
        const res = await API.getPlaylistDetail(playlist.id);
        if (res.playlist) {
          this.currentPlaylistDetail = res.playlist;
          this.musicList = res.playlist.tracks || [];
          this.activeTab = 'search';
          this.showToast(`已载入歌单: ${playlist.name} (已过滤不可播放歌曲)`);
        }
      } catch (e) {
        this.showToast('加载歌单详情失败');
      }
    },

    // ==================== 播放与核心控制 ====================
    async playMusic(song, list = null) {
      if (!song) return;

      if (list) {
        this.playQueue = [...list];
        this.currentQueueIndex = this.playQueue.findIndex(s => s.id === song.id);
      } else if (!this.playQueue.some(s => s.id === song.id)) {
        this.playQueue.push(song);
        this.currentQueueIndex = this.playQueue.length - 1;
      }

      const songId = song.id;
      const songName = song.name || '未知歌曲';
      const artist = (song.ar && song.ar[0] ? song.ar[0].name : (song.artists && song.artists[0] ? song.artists[0].name : (song.artist || '未知歌手')));
      const album = (song.al ? song.al.name : (song.album ? (song.album.name || song.album) : ''));
      const cover = (song.al && song.al.picUrl ? song.al.picUrl : (song.picUrl ? song.picUrl : 'images/cover.png'));
      const duration = (song.dt ? Math.floor(song.dt / 1000) : (song.duration || 0));

      let playUrl = song.play_url || '';

      // 如果未携带直链，按音质拉取
      if (!playUrl) {
        try {
          const urlRes = await API.getSongUrl(songId, this.soundQuality);
          if (urlRes.url) {
            playUrl = urlRes.url;
          }
        } catch (e) {}
      }

      if (!playUrl) {
        this.showToast('该歌曲暂无可用播放链接，为您自动播放下一首');
        this.playNext();
        return;
      }

      this.currentSong = {
        id: songId,
        name: songName,
        artist: artist,
        album: album,
        cover: cover,
        url: playUrl,
        duration: duration
      };

      const audio = this.$refs.audio;
      audio.src = playUrl;
      audio.play().then(() => {
        this.isPlay = true;
      }).catch(() => {
        this.isPlay = false;
      });

      // 加载歌词 (LRU + MySQL)
      this.loadLyric(songId);

      // 检查是否收藏
      if (this.currentUser) {
        this.checkFavoriteState(songId);
        // 记录播放历史
        API.recordHistory({
          song_id: songId,
          name: songName,
          artist: artist,
          cover_url: cover,
          duration: duration
        });
      }
    },

    togglePlay() {
      const audio = this.$refs.audio;
      if (!audio.src) {
        if (this.musicList.length > 0) {
          this.playMusic(this.musicList[0], this.musicList);
        }
        return;
      }
      if (this.isPlay) {
        audio.pause();
        this.isPlay = false;
      } else {
        audio.play();
        this.isPlay = true;
      }
    },

    playPrev() {
      if (this.playQueue.length === 0) return;
      let nextIdx = this.currentQueueIndex - 1;
      if (nextIdx < 0) nextIdx = this.playQueue.length - 1;
      this.currentQueueIndex = nextIdx;
      this.playMusic(this.playQueue[nextIdx]);
    },

    playNext() {
      if (this.playQueue.length === 0) return;
      let nextIdx = 0;
      if (this.playMode === 'random') {
        nextIdx = Math.floor(Math.random() * this.playQueue.length);
      } else {
        nextIdx = (this.currentQueueIndex + 1) % this.playQueue.length;
      }
      this.currentQueueIndex = nextIdx;
      this.playMusic(this.playQueue[nextIdx]);
    },

    togglePlayMode() {
      const modes = ['order', 'single', 'random'];
      const curIdx = modes.indexOf(this.playMode);
      this.playMode = modes[(curIdx + 1) % modes.length];
      const modeNames = { order: '顺序播放', single: '单曲循环', random: '随机播放' };
      this.showToast(`播放模式切换为：${modeNames[this.playMode]}`);
    },

    onProgressChange(e) {
      const percent = parseFloat(e.target.value);
      const audio = this.$refs.audio;
      if (audio && audio.duration) {
        audio.currentTime = (percent / 100) * audio.duration;
      }
    },

    onVolumeChange(e) {
      this.volume = parseFloat(e.target.value);
      const audio = this.$refs.audio;
      if (audio) audio.volume = this.volume;
    },

    changeSoundQuality(level) {
      this.soundQuality = level;
      this.showToast(`音质已切换为：${level}`);
      if (this.currentSong.id) {
        this.playMusic(this.currentSong);
      }
    },

    // ==================== 歌词解析与平滑滚动 ====================
    async loadLyric(songId) {
      this.rawLyrics = [];
      this.currentLyricIndex = 0;
      try {
        const res = await API.getSongLyric(songId);
        if (res.lrc && res.lrc.lyric) {
          this.rawLyrics = this.parseLrc(res.lrc.lyric);
        } else {
          this.rawLyrics = [{ time: 0, text: '纯音乐，请您静心聆听' }];
        }
      } catch (e) {
        this.rawLyrics = [{ time: 0, text: '暂无歌词' }];
      }
    },

    parseLrc(lrcStr) {
      const lines = lrcStr.split('\n');
      const timeExp = /\[(\d{2}):(\d{2})(?:\.(\d{2,3}))?\]/;
      const result = [];

      for (const line of lines) {
        const match = timeExp.exec(line);
        if (match) {
          const min = parseInt(match[1], 10);
          const sec = parseInt(match[2], 10);
          const ms = match[3] ? parseInt(match[3].padEnd(3, '0'), 10) : 0;
          const totalSec = min * 60 + sec + ms / 1000;
          const text = line.replace(timeExp, '').trim();
          if (text) {
            result.push({ time: totalSec, text });
          }
        }
      }
      return result.sort((a, b) => a.time - b.time);
    },

    initAudioEvents() {
      const audio = this.$refs.audio;
      if (!audio) return;

      audio.addEventListener('timeupdate', () => {
        this.currentTime = audio.currentTime;
        this.totalDuration = audio.duration || this.currentSong.duration || 0;
        if (this.totalDuration > 0) {
          this.progressPercent = (this.currentTime / this.totalDuration) * 100;
        }
        this.updateLyricPosition(this.currentTime);
      });

      audio.addEventListener('ended', () => {
        if (this.playMode === 'single') {
          audio.currentTime = 0;
          audio.play();
        } else {
          this.playNext();
        }
      });
    },

    updateLyricPosition(time) {
      if (!this.rawLyrics.length) return;
      let targetIndex = 0;
      for (let i = 0; i < this.rawLyrics.length; i++) {
        if (time >= this.rawLyrics[i].time) {
          targetIndex = i;
        } else {
          break;
        }
      }
      this.currentLyricIndex = targetIndex;
    },

    // ==================== 收藏与历史 ====================
    async checkFavoriteState(songId) {
      try {
        const res = await API.checkFavorite(songId);
        this.isCurrentSongFav = !!res.is_favorite;
      } catch (e) {
        this.isCurrentSongFav = false;
      }
    },

    async toggleCurrentFavorite(song = null) {
      if (!this.currentUser) {
        this.openAuth('login');
        this.showToast('请先登录后再收藏歌曲');
        return;
      }

      const target = song || this.currentSong;
      if (!target || !target.id) return;

      try {
        const artist = target.ar ? target.ar[0].name : (target.artist || '未知歌手');
        const album = target.al ? target.al.name : (target.album || '');
        const cover = target.al ? target.al.picUrl : (target.picUrl || target.cover || '');

        const res = await API.toggleFavorite({
          song_id: target.id,
          name: target.name,
          artist: artist,
          album: album,
          cover_url: cover,
          duration: target.duration || 0
        });

        if (res.code === 200) {
          if (!song || song.id === this.currentSong.id) {
            this.isCurrentSongFav = res.is_favorite;
          }
          this.showToast(res.message);
          this.loadFavorites();
        }
      } catch (e) {
        this.showToast('操作收藏失败');
      }
    },

    async loadFavorites() {
      if (!this.currentUser) return;
      try {
        const res = await API.getFavorites();
        if (res.code === 200) {
          this.favoritesList = res.favorites || [];
        }
      } catch (e) {}
    },

    async loadHistory() {
      if (!this.currentUser) return;
      try {
        const res = await API.getHistory();
        if (res.code === 200) {
          this.historyList = res.history || [];
        }
      } catch (e) {}
    },

    // ==================== 🤖 AI Agent 智能推荐 ====================
    usePresetChip(chipText) {
      this.agentPrompt = chipText.replace(/^[^\s]+\s*/, '');
      this.requestAgentRecommendation();
    },

    async requestAgentRecommendation() {
      if (!this.agentPrompt.trim()) {
        this.showToast('请输入您的听歌场景或心情');
        return;
      }

      this.agentLoading = true;
      try {
        const res = await API.agentRecommend({
          prompt: this.agentPrompt.trim(),
          mood: this.agentMood,
          genre: this.agentGenre
        });

        if (res.code === 200) {
          this.agentReply = res;
          this.showToast('AI Agent 个性化推荐生成完毕');
        } else {
          this.showToast('Agent 推荐生成失败');
        }
      } catch (err) {
        this.showToast('请求 AI Agent 出错');
      } finally {
        this.agentLoading = false;
      }
    },

    playAllAgentSongs() {
      if (this.agentReply && this.agentReply.songs && this.agentReply.songs.length > 0) {
        this.playMusic(this.agentReply.songs[0], this.agentReply.songs);
        this.showToast('已开始连续播放 AI 推荐列表');
      }
    }
  }
});
