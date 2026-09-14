-- ========================================================
-- 音乐播放器 (Demo Player) 数据库初始化脚本
-- 适配 MySQL 8.0+ / 5.7+
-- ========================================================

CREATE DATABASE IF NOT EXISTS `demo_player` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE `demo_player`;

-- 1. 用户信息表
CREATE TABLE IF NOT EXISTS `users` (
    `id` BIGINT AUTO_INCREMENT PRIMARY KEY,
    `username` VARCHAR(64) NOT NULL UNIQUE COMMENT '用户名',
    `phone_email` VARCHAR(128) DEFAULT '' COMMENT '手机或邮箱',
    `password_hash` VARCHAR(128) NOT NULL COMMENT '加盐SHA256哈希密码',
    `nickname` VARCHAR(64) DEFAULT '音乐玩家' COMMENT '昵称',
    `avatar_url` VARCHAR(512) DEFAULT 'images/person.png' COMMENT '头像URL',
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX `idx_username` (`username`),
    INDEX `idx_phone_email` (`phone_email`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户账号表';

-- 2. 用户收藏歌曲表
CREATE TABLE IF NOT EXISTS `favorites` (
    `id` BIGINT AUTO_INCREMENT PRIMARY KEY,
    `user_id` BIGINT NOT NULL COMMENT '用户ID',
    `song_id` BIGINT NOT NULL COMMENT '网易云歌曲ID',
    `song_name` VARCHAR(256) NOT NULL COMMENT '歌曲名称',
    `artist_name` VARCHAR(256) DEFAULT '' COMMENT '歌手名称',
    `album_name` VARCHAR(256) DEFAULT '' COMMENT '专辑名称',
    `cover_url` VARCHAR(512) DEFAULT '' COMMENT '封面图片URL',
    `duration` INT DEFAULT 0 COMMENT '歌曲时长(秒)',
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY `uk_user_song` (`user_id`, `song_id`),
    INDEX `idx_user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户歌曲收藏表';

-- 3. 用户自建歌单表
CREATE TABLE IF NOT EXISTS `user_playlists` (
    `id` BIGINT AUTO_INCREMENT PRIMARY KEY,
    `user_id` BIGINT NOT NULL COMMENT '创建者用户ID',
    `name` VARCHAR(128) NOT NULL COMMENT '歌单名称',
    `cover_url` VARCHAR(512) DEFAULT 'images/cover.png' COMMENT '歌单封面',
    `description` VARCHAR(512) DEFAULT '' COMMENT '歌单描述',
    `play_count` INT DEFAULT 0 COMMENT '播放次数',
    `is_public` TINYINT(1) DEFAULT 1 COMMENT '是否公开: 1公开 0私密',
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX `idx_user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户自建歌单表';

-- 4. 歌单-歌曲关联表
CREATE TABLE IF NOT EXISTS `playlist_tracks` (
    `id` BIGINT AUTO_INCREMENT PRIMARY KEY,
    `playlist_id` BIGINT NOT NULL COMMENT '歌单ID',
    `song_id` BIGINT NOT NULL COMMENT '歌曲ID',
    `song_name` VARCHAR(256) NOT NULL COMMENT '歌曲名称',
    `artist_name` VARCHAR(256) DEFAULT '' COMMENT '歌手名称',
    `album_name` VARCHAR(256) DEFAULT '' COMMENT '专辑名称',
    `cover_url` VARCHAR(512) DEFAULT '' COMMENT '封面',
    `duration` INT DEFAULT 0 COMMENT '时长',
    `added_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY `uk_playlist_song` (`playlist_id`, `song_id`),
    INDEX `idx_playlist_id` (`playlist_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='歌单歌曲明细表';

-- 5. 歌词缓存表 (仅存轻量文本，毫秒级快速读取)
CREATE TABLE IF NOT EXISTS `lyrics_cache` (
    `song_id` BIGINT PRIMARY KEY COMMENT '歌曲ID',
    `lyric_text` MEDIUMTEXT NOT NULL COMMENT 'LRC歌词原始文本',
    `tlyric_text` MEDIUMTEXT DEFAULT NULL COMMENT '翻译歌词文本',
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='歌词轻量持久化缓存';

-- 6. 用户播放历史表
CREATE TABLE IF NOT EXISTS `play_history` (
    `id` BIGINT AUTO_INCREMENT PRIMARY KEY,
    `user_id` BIGINT NOT NULL COMMENT '用户ID',
    `song_id` BIGINT NOT NULL COMMENT '歌曲ID',
    `song_name` VARCHAR(256) NOT NULL COMMENT '歌曲名称',
    `artist_name` VARCHAR(256) DEFAULT '' COMMENT '歌手名称',
    `cover_url` VARCHAR(512) DEFAULT '' COMMENT '封面图片',
    `duration` INT DEFAULT 0 COMMENT '时长',
    `played_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX `idx_user_played` (`user_id`, `played_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户播放历史';

-- 7. 用户画像偏好表 (供 AI Agent 个性化推荐)
CREATE TABLE IF NOT EXISTS `user_preferences` (
    `user_id` BIGINT PRIMARY KEY COMMENT '用户ID',
    `fav_genres` VARCHAR(512) DEFAULT '流行,民谣,轻音乐' COMMENT '偏好流派',
    `fav_artists` VARCHAR(512) DEFAULT '' COMMENT '常听歌手列表',
    `mood_tags` VARCHAR(512) DEFAULT '放松,专注' COMMENT '常选心情偏好',
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户画像偏好';

-- 插入一个默认管理员/测试用户 (密码: 123456 -> 加盐SHA256)
-- 密码明文: 123456, salt: demo_salt_2026, sha256: 8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92
INSERT INTO `users` (`username`, `phone_email`, `password_hash`, `nickname`, `avatar_url`)
VALUES ('admin', 'admin@demoplayer.com', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '音乐探险家', 'images/person.png')
ON DUPLICATE KEY UPDATE `nickname`=VALUES(`nickname`);
