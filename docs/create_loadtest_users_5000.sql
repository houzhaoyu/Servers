-- 为 ChatApp 创建 5000 名专用压测用户。
--
-- 账号格式：
--   用户名：loadtest0001 ~ loadtest5000
--   邮箱：  loadtest0001@example.com ~ loadtest5000@example.com
--   密码：  123456
--
-- 特性：
--   1. UID 从 user.uid 和 user_id.id 的当前最大值之后开始分配；
--   2. 使用 INSERT IGNORE，可重复执行，不覆盖已有账号；
--   3. 完成后同步 user_id，避免后续正常注册发生 UID 冲突；
--   4. 全部写入位于同一个 InnoDB 事务中。

USE ChatApp;

-- user_id 按项目设计应当只有一行。若初始化不完整导致该表为空，先补齐序列值。
INSERT INTO `user_id` (`id`)
SELECT COALESCE(MAX(`uid`), 100000)
FROM `user`
WHERE NOT EXISTS (SELECT 1 FROM `user_id`);

START TRANSACTION;

-- 获取 user_id 行锁，阻止注册存储过程在批量写入期间分配重复 UID。
UPDATE `user_id` SET `id` = `id`;

SET @base_uid := GREATEST(
    COALESCE((SELECT MAX(`uid`) FROM `user`), 100000),
    COALESCE((SELECT MAX(`id`) FROM `user_id`), 100000)
);

-- 四个 0~9 数字表笛卡尔积产生 0~9999，本次只取前 5000 个序号。
INSERT IGNORE INTO `user`
    (`uid`, `name`, `email`, `pwd`, `nick`, `desc`, `sex`, `icon`)
SELECT
    @base_uid + numbers.seq                             AS `uid`,
    CONCAT('loadtest', LPAD(numbers.seq, 4, '0'))       AS `name`,
    CONCAT('loadtest', LPAD(numbers.seq, 4, '0'),
           '@example.com')                             AS `email`,
    '123456'                                           AS `pwd`,
    CONCAT('压测用户', LPAD(numbers.seq, 4, '0'))       AS `nick`,
    '自动生成的压测账号'                                AS `desc`,
    MOD(numbers.seq, 2)                                AS `sex`,
    ''                                                 AS `icon`
FROM (
    SELECT
        ones.n
        + tens.n * 10
        + hundreds.n * 100
        + thousands.n * 1000
        + 1 AS seq
    FROM
        (SELECT 0 n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3
         UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6
         UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) ones
    CROSS JOIN
        (SELECT 0 n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3
         UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6
         UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) tens
    CROSS JOIN
        (SELECT 0 n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3
         UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6
         UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) hundreds
    CROSS JOIN
        (SELECT 0 n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3
         UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6
         UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) thousands
) numbers
WHERE numbers.seq <= 5000
ORDER BY numbers.seq;

SET @inserted_rows := ROW_COUNT();
SET @max_uid := (SELECT COALESCE(MAX(`uid`), @base_uid) FROM `user`);

-- 正常情况下 user_id 只有一行；即使意外存在多行，也统一到安全的最大 UID。
UPDATE `user_id` SET `id` = @max_uid;

COMMIT;

-- 执行结果与账号范围校验。
SELECT
    @inserted_rows AS `本次新增用户数`,
    COUNT(*) AS `当前压测用户总数`,
    MIN(`uid`) AS `压测用户最小UID`,
    MAX(`uid`) AS `压测用户最大UID`
FROM `user`
WHERE `name` REGEXP '^loadtest[0-9]{4}$'
  AND `email` REGEXP '^loadtest[0-9]{4}@example\\.com$';

SELECT `id` AS `下一次注册前的UID序列值`
FROM `user_id`;

-- 如需生成压测程序 users.csv，可执行：
-- SELECT 'email' AS email, 'password' AS password
-- UNION ALL
-- SELECT email, pwd
-- FROM user
-- WHERE name REGEXP '^loadtest[0-9]{4}$'
-- ORDER BY email;
