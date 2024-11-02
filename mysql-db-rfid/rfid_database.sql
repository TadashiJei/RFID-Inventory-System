-- Create the database
CREATE DATABASE IF NOT EXISTS rfid_inventory;
USE rfid_inventory;

-- Create the inventory table
CREATE TABLE IF NOT EXISTS inventory (
  id VARCHAR(50) PRIMARY KEY,
  name VARCHAR(100) NOT NULL,
  quantity INT NOT NULL,
  last_scanned BIGINT
);

-- Create the recent_scans table
CREATE TABLE IF NOT EXISTS recent_scans (
  id INT AUTO_INCREMENT PRIMARY KEY,
  scan_time BIGINT NOT NULL
);

-- Create a user for the ESP32 to connect
CREATE USER 'rfid_user'@'%' IDENTIFIED BY 'rfid_password';
GRANT ALL PRIVILEGES ON rfid_inventory.* TO 'rfid_user'@'%';
FLUSH PRIVILEGES;

-- Add some sample data
INSERT INTO inventory (id, name, quantity, last_scanned) VALUES
('0123456789', 'Sample Item 1', 10, UNIX_TIMESTAMP() * 1000),
('9876543210', 'Sample Item 2', 5, UNIX_TIMESTAMP() * 1000);