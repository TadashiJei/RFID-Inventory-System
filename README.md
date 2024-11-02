# RFID Inventory Management System

A real-time inventory management system built with ESP32, RFID reader (MFRC522), and a web interface. The system allows for seamless tracking of inventory items using RFID tags, with data persistence in MySQL and a responsive web dashboard.

## 🚀 Features

- Real-time RFID tag scanning and inventory updates
- Web-based dashboard for inventory management
- WebSocket support for live updates
- MySQL database integration for data persistence
- Recent scans history
- Responsive design supporting desktop and mobile views
- Easy item addition and management interface

## 📋 Prerequisites

- ESP32 development board
- MFRC522 RFID reader
- MySQL Server (5.7+ or MariaDB)
- Arduino IDE with ESP32 board support
- Required Arduino Libraries:
  - WiFi
  - SPIFFS
  - ESPAsyncWebServer
  - AsyncTCP
  - ArduinoJson
  - MFRC522
  - MySQL Connector Arduino

## 🔧 Hardware Setup

Connect the MFRC522 RFID reader to ESP32:
- SDA (SS) -> GPIO 5
- SCK -> GPIO 18
- MOSI -> GPIO 23
- MISO -> GPIO 19
- RST -> GPIO 27
- VCC -> 3.3V
- GND -> GND

## ⚙️ Software Setup

1. Clone this repository:
```bash
git clone https://github.com/TadashiJei/RFID-Inventory-System.git
```

2. Set up the MySQL database:
```bash
mysql -u root -p < rfid_database.sql
```

3. Configure the ESP32:
   - Open `main.ino` in Arduino IDE
   - Update WiFi credentials:
     ```cpp
     const char* ssid = "YOUR_WIFI_SSID";
     const char* password = "YOUR_WIFI_PASSWORD";
     ```
   - Update MySQL connection settings:
     ```cpp
     IPAddress server_addr(192,168,1,100);  // Your MySQL server IP
     char user[] = "rfid_user";
     char password_mysql[] = "rfid_password";
     ```

4. Upload the code to ESP32:
   - Select your ESP32 board in Arduino IDE
   - Upload the SPIFFS data
   - Upload the main sketch

## 🌐 System Integration Options

### Local MySQL Server
- Default setup using a local MySQL instance
- Ideal for standalone deployments
- Configure firewall rules to allow ESP32 connection

### Cloud Database Services
The system can be integrated with various cloud database services:
- Amazon RDS
- Google Cloud SQL
- Azure Database for MySQL
- DigitalOcean Managed Databases

To use cloud databases, update the connection settings in `main.ino`:
```cpp
IPAddress server_addr(YOUR_CLOUD_DB_IP);
char user[] = "your_cloud_db_user";
char password_mysql[] = "your_cloud_db_password";
```

### Enterprise Integration
For enterprise environments:
- Set up a VPN tunnel for secure database access
- Implement SSL/TLS encryption for database connections
- Use environment-specific configuration files

## 📱 Web Interface

Access the dashboard by navigating to the ESP32's IP address in your web browser:
```
http://[ESP32_IP_ADDRESS]
```

Features:
- Real-time inventory view
- Recent scans history
- Add new items manually
- RFID tag registration
- Quantity updates

## 🤝 Contributing

Contributions are welcome! Here's how you can help:

1. Fork the repository
2. Create your feature branch:
```bash
git checkout -b feature/AmazingFeature
```
3. Commit your changes:
```bash
git commit -m 'Add some AmazingFeature'
```
4. Push to the branch:
```bash
git push origin feature/AmazingFeature
```
5. Open a Pull Request

### Areas for Contribution
- Additional cloud service integrations
- Enhanced security features
- UI/UX improvements
- Documentation translations
- Bug fixes and optimizations
- New features and capabilities

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🔍 Troubleshooting

Common issues and solutions:
- Database connection fails: Check network connectivity and firewall settings
- RFID reader not responding: Verify SPI connections and pins
- Web interface not loading: Ensure SPIFFS is properly uploaded
- Real-time updates not working: Check WebSocket connection

## 📞 Support

For support and questions:
- Open an issue in the GitHub repository
- Join our [Discord community](tadashijei.com/discord)
- Check out the [Wiki](wiki.tadashijei.com) for detailed documentation

## ⭐ Star History

If you like this project, please give it a star! It helps us know that you find it useful and encourages further development.
