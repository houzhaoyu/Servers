CREATE DATABASE IF NOT EXISTS ChatApp DEFAULT CHARACTER SET utf8mb4;
USE ChatApp;

CREATE TABLE IF NOT EXISTS `user` (
  `uid`   INT NOT NULL,
  `name`  VARCHAR(64)  NOT NULL,
  `email` VARCHAR(128) NOT NULL,
  `pwd`   VARCHAR(128) NOT NULL,
  `nick`  VARCHAR(64)  DEFAULT '',
  `desc`  VARCHAR(255) DEFAULT '',
  `sex`   TINYINT      DEFAULT 0,
  `icon`  VARCHAR(255) DEFAULT '',
  PRIMARY KEY (`uid`),
  UNIQUE KEY `uk_name` (`name`),
  UNIQUE KEY `uk_email` (`email`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `user_id` (
  `id` INT NOT NULL
) ENGINE=InnoDB;
INSERT INTO `user_id` (`id`) VALUES (100000);

CREATE TABLE IF NOT EXISTS `friend_apply` (
  `id`        INT NOT NULL AUTO_INCREMENT,
  `from_uid`  INT NOT NULL,
  `to_uid`    INT NOT NULL,
  `descs`     VARCHAR(255) DEFAULT '',
  `back_name` VARCHAR(64)  DEFAULT '',
  `status`    TINYINT      DEFAULT 0,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_from_to` (`from_uid`, `to_uid`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `friend` (
  `self_id`   INT NOT NULL,
  `friend_id` INT NOT NULL,
  `back`      VARCHAR(64) DEFAULT '',
  PRIMARY KEY (`self_id`, `friend_id`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `chat_thread` (
  `thread_id`  BIGINT NOT NULL AUTO_INCREMENT,
  `type`       VARCHAR(16) NOT NULL,
  `created_at` DATETIME DEFAULT NULL,
  PRIMARY KEY (`thread_id`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `private_chat` (
  `thread_id`  BIGINT NOT NULL,
  `user1_id`   INT NOT NULL,
  `user2_id`   INT NOT NULL,
  `created_at` DATETIME DEFAULT NULL,
  PRIMARY KEY (`thread_id`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `group_chat_member` (
  `thread_id` BIGINT NOT NULL,
  `user_id`   INT NOT NULL,
  PRIMARY KEY (`thread_id`, `user_id`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `chat_message` (
  `message_id` BIGINT NOT NULL AUTO_INCREMENT,
  `thread_id`  BIGINT NOT NULL,
  `sender_id`  BIGINT NOT NULL,
  `recv_id`    BIGINT NOT NULL,
  `content`    TEXT,
  `created_at` DATETIME DEFAULT NULL,
  `updated_at` DATETIME DEFAULT NULL,
  `status`     TINYINT  DEFAULT 0,
  `msg_type`   TINYINT  DEFAULT 0,
  PRIMARY KEY (`message_id`),
  KEY `idx_thread` (`thread_id`)
) ENGINE=InnoDB;

DELIMITER $$
CREATE PROCEDURE `reg_user`(
  IN  p_name  VARCHAR(64),
  IN  p_email VARCHAR(128),
  IN  p_pwd   VARCHAR(128),
  OUT result   INT
)
BEGIN
  DECLARE cnt INT DEFAULT 0;
  DECLARE new_id INT DEFAULT 0;

  SELECT COUNT(*) INTO cnt FROM `user` WHERE `email` = p_email;
  IF cnt > 0 THEN
    SET result = 0;
  ELSE
    SELECT COUNT(*) INTO cnt FROM `user` WHERE `name` = p_name;
    IF cnt > 0 THEN
      SET result = 0;
    ELSE
      UPDATE `user_id` SET `id` = `id` + 1;
      SELECT `id` INTO new_id FROM `user_id` LIMIT 1;
      INSERT INTO `user`(`uid`, `name`, `email`, `pwd`) VALUES (new_id, p_name, p_email, p_pwd);
      SET result = new_id;
    END IF;
  END IF;
END$$

CREATE PROCEDURE `test_procedure`(
  IN  p_email  VARCHAR(128),
  OUT userId   INT,
  OUT userName VARCHAR(64)
)
BEGIN
  SELECT `uid`, `name` INTO userId, userName FROM `user` WHERE `email` = p_email LIMIT 1;
END$$
DELIMITER ;