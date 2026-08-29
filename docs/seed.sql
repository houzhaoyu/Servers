-- 测试种子数据：5 个测试用户
-- 注意：本项目 CheckPwd 为明文比对，密码直接存明文（仅用于本地测试）
USE ChatApp;

-- 5 个测试用户，uid 从 100001 开始，与 init.sql 中 user_id 初始值 100000 衔接
INSERT IGNORE INTO `user` (`uid`, `name`, `email`, `pwd`, `nick`, `desc`, `sex`, `icon`) VALUES
(100001, 'user1', 'user1@example.com', '123456', '用户一', '', 0, ''),
(100002, 'user2', 'user2@example.com', '123456', '用户二', '', 0, ''),
(100003, 'user3', 'user3@example.com', '123456', '用户三', '', 0, ''),
(100004, 'user4', 'user4@example.com', '123456', '用户四', '', 0, ''),
(100005, 'user5', 'user5@example.com', '123456', '用户五', '', 0, '');

-- 同步自增序号，避免后续注册的新用户 uid 与种子数据冲突
DELETE FROM `user_id`;
INSERT INTO `user_id` (`id`) VALUES (100005);