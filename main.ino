// ===== ESP32 Multi-Sensor System with Correct Turbidity Calculation =====

#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>

// WiFi credentials
const char* ssid = "Agriproject";
const char* password = "agri@123";

// I2C LCD Configuration
#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

// Sensor Pins
#define PH_PIN 4
#define ONE_WIRE_BUS 6
#define TDS_PIN 0
#define TURBIDITY_PIN 1

// Sensor Objects
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Sensor Variables
float ph_value = 7.0;
float temperature = 25.0;
float tds_value = 0.0;
float turbidity_value = 0.0;

// Global variables for turbidity
int turbidity_raw = 0;
float turbidity_voltage = 0.0;

// Calibration Values
float ph_calibration = 24.0;
float tds_calibration = 0.5;

// Web Server
WebServer server(80);

// LCD Display Variables
unsigned long lastDisplayUpdate = 0;
int displayMode = 0;
const int DISPLAY_INTERVAL = 5000;

// HTML Page
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 Water Quality Monitor</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            margin: 0;
            padding: 20px;
            background-color: #f0f8ff;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            padding: 30px;
            border-radius: 15px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.1);
        }
        .header {
            color: #2c3e50;
            margin-bottom: 30px;
            padding-bottom: 10px;
        }
        .sensor-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin: 30px 0;
        }
        .sensor-box {
            padding: 25px;
            border-radius: 10px;
            color: white;
            font-size: 24px;
            font-weight: bold;
            text-shadow: 1px 1px 2px rgba(0,0,0,0.3);
            min-height: 180px;
            display: flex;
            flex-direction: column;
            justify-content: center;
        }
        .ph-box { background: linear-gradient(135deg, #3498db, #2980b9); }
        .temp-box { background: linear-gradient(135deg, #e74c3c, #c0392b); }
        .tds-box { background: linear-gradient(135deg, #2ecc71, #27ae60); }
        .turbidity-box { background: linear-gradient(135deg, #9b59b6, #8e44ad); }
        
        .value {
            font-size: 42px;
            margin: 10px 0;
        }
        .unit {
            font-size: 20px;
            opacity: 0.9;
            margin-top: 5px;
        }
        .raw-value {
            font-size: 16px;
            margin-top: 10px;
            opacity: 0.8;
        }
        .analysis-box {
            background: #f8f9fa;
            padding: 20px;
            border-radius: 10px;
            margin: 25px 0;
            text-align: left;
            border-left: 5px solid #3498db;
        }
        .analysis-title {
            color: #2c3e50;
            font-size: 18px;
            font-weight: bold;
            margin-bottom: 10px;
        }
        .analysis-item {
            margin: 8px 0;
            padding: 8px;
            background: white;
            border-radius: 5px;
        }
        .timestamp {
            color: #7f8c8d;
            font-size: 14px;
            margin-top: 20px;
            padding-top: 15px;
            border-top: 1px solid #eee;
        }
        .controls {
            display: flex;
            justify-content: center;
            gap: 15px;
            margin: 25px 0;
            flex-wrap: wrap;
        }
        .btn {
            background: #3498db;
            color: white;
            border: none;
            padding: 12px 25px;
            font-size: 16px;
            border-radius: 50px;
            cursor: pointer;
            transition: all 0.3s;
            min-width: 180px;
        }
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 8px rgba(0,0,0,0.2);
        }
        .btn-refresh { background: #2ecc71; }
        .btn-refresh:hover { background: #27ae60; }
        .btn-stop { background: #e74c3c; }
        .btn-stop:hover { background: #c0392b; }
        .status-panel {
            background: #e8f4fc;
            padding: 15px;
            border-radius: 10px;
            margin: 20px 0;
            color: #2c5282;
            font-size: 14px;
        }
        .status-good {
            color: #27ae60;
            font-weight: bold;
        }
        .status-warning {
            color: #e67e22;
            font-weight: bold;
        }
        .status-danger {
            color: #e74c3c;
            font-weight: bold;
        }
        .sensor-status {
            font-size: 14px;
            margin-top: 10px;
            min-height: 20px;
        }
        .turbidity-info {
            font-size: 12px;
            margin-top: 5px;
            color: rgba(255,255,255,0.9);
        }
    </style>
    <script>
        let autoRefresh = true;
        let refreshInterval;
        
        function updateSensorData(data) {
            // Update sensor values
            document.getElementById('phValue').innerHTML = data.ph.toFixed(2);
            document.getElementById('tempValue').innerHTML = data.temp.toFixed(1);
            document.getElementById('tdsValue').innerHTML = data.tds.toFixed(0);
            document.getElementById('turbidityValue').innerHTML = data.turbidity.toFixed(0);
            
            // Update raw values display
            document.getElementById('turbidityRaw').innerHTML = 
                'Raw ADC: ' + data.turbidity_raw + ' (' + data.turbidity_voltage.toFixed(3) + 'V)';
            
            // Update status messages
            document.getElementById('phStatus').innerHTML = getPHStatus(data.ph);
            document.getElementById('tempStatus').innerHTML = getTempStatus(data.temp);
            document.getElementById('tdsStatus').innerHTML = getTDSStatus(data.tds);
            document.getElementById('turbidityStatus').innerHTML = getTurbidityStatus(data.turbidity);
            document.getElementById('nutrientStatus').innerHTML = getNutrientStatus(data.ph, data.temp);
            
            // Update timestamp
            document.getElementById('timestamp').innerHTML = new Date().toLocaleTimeString();
            
            // Update connection status
            document.getElementById('connectionStatus').innerHTML = 
                '<span class="status-good">Connected</span> | IP: ' + data.ip;
        }
        
        function getPHStatus(pH) {
            if(pH >= 6.0 && pH <= 7.5) return '<span class="status-good">Optimal for nutrients</span>';
            if(pH < 6.0) return '<span class="status-danger">Too acidic - Add lime</span>';
            return '<span class="status-danger">Too alkaline - Add sulfur</span>';
        }
        
        function getTempStatus(temp) {
            if(temp >= 18 && temp <= 28) return '<span class="status-good">Ideal for plant growth</span>';
            if(temp < 15) return '<span class="status-warning">Too cold - Slow uptake</span>';
            return '<span class="status-warning">Too hot - Risk of stress</span>';
        }
        
        function getTDSStatus(tds) {
            if(tds < 300) return '<span class="status-warning">Low minerals - May need fertilizer</span>';
            if(tds > 1500) return '<span class="status-danger">High TDS - Consider flushing</span>';
            return '<span class="status-good">Good mineral level</span>';
        }
        
        function getTurbidityStatus(turbidity) {
            // Higher number = more turbid (cloudy)
            // Lower number = clearer water
            if(turbidity < 300) {
                return '<span class="status-good">Crystal clear water</span>';
            } else if(turbidity < 700) {
                return '<span class="status-good">Slightly cloudy</span>';
            } else if(turbidity < 1300) {
                return '<span class="status-warning">Cloudy water</span>';
            } else {
                return '<span class="status-danger">Very turbid - Filter needed</span>';
            }
        }
        
function getNutrientStatus(pH, temp) {
    let status = [];
    
    if(pH >= 6.0 && pH <= 7.0) {
        status.push("&#10004; Nitrogen: High availability");
    } else {
        status.push("&#10006; Nitrogen: Low availability");
    }
    
    if(pH >= 6.5 && pH <= 7.5) {
        status.push("&#10004; Phosphorus: High availability");
    } else {
        status.push("&#10006; Phosphorus: Low availability");
    }
    
    if(pH >= 6.0 && pH <= 7.5) {
        status.push("&#10004; Potassium: High availability");
    } else {
        status.push("&#10006; Potassium: Low availability");
    }
    
    if(temp >= 20 && temp <= 25) {
        status.push("&#10004; Nutrient uptake: Optimal");
    } else if(temp < 15) {
        status.push("&#10006; Nutrient uptake: Reduced");
    } else {
        status.push("&#9888; Nutrient uptake: Suboptimal");
    }
    
    return status.join("<br>");
}
        
        function fetchData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    updateSensorData(data);
                })
                .catch(error => {
                    console.error('Error:', error);
                    document.getElementById('connectionStatus').innerHTML = 
                        '<span class="status-danger">Disconnected</span>';
                });
        }
        
        function toggleRefresh() {
            autoRefresh = !autoRefresh;
            const btn = document.getElementById('toggleBtn');
            if(autoRefresh) {
                btn.innerHTML = 'Stop Auto Refresh';
                btn.className = 'btn btn-stop';
                refreshInterval = setInterval(fetchData, 2000);
            } else {
                btn.innerHTML = 'Start Auto Refresh';
                btn.className = 'btn btn-refresh';
                clearInterval(refreshInterval);
            }
        }
        
        // Initial load
        window.onload = function() {
            fetchData();
            refreshInterval = setInterval(fetchData, 2000);
        }
    </script>
</head>
<body>
    <div class="container">
        <h1 class="header">ESP32 Water Quality Monitor</h1>
        
        <div class="status-panel">
            <div id="connectionStatus">Connecting to server...</div>
        </div>
        
        <div class="sensor-grid">
            <div class="sensor-box ph-box">
                <h3>pH Value</h3>
                <div class="value" id="phValue">--.--</div>
                <div class="unit">pH</div>
                <div class="sensor-status" id="phStatus">--</div>
            </div>
            
            <div class="sensor-box temp-box">
                <h3>Temperature</h3>
                <div class="value" id="tempValue">--.-</div>
                <div class="unit">&deg;C</div>
                <div class="sensor-status" id="tempStatus">--</div>
            </div>
            
            <div class="sensor-box tds-box">
                <h3>TDS Value</h3>
                <div class="value" id="tdsValue">---</div>
                <div class="unit">ppm</div>
                <div class="sensor-status" id="tdsStatus">--</div>
            </div>
            
            <div class="sensor-box turbidity-box">
                <h3>Turbidity</h3>
                <div class="value" id="turbidityValue">---</div>
                <div class="unit">Turbidity Units</div>
                <div class="turbidity-info" id="turbidityRaw">Raw: -- ADC (--.--V)</div>
                <div class="sensor-status" id="turbidityStatus">--</div>
            </div>
        </div>
        
        <div class="analysis-box">
            <div class="analysis-title">Turbidity Guide</div>
            <div class="analysis-item">
              &bull; <span style="color:#27ae60">0-300</span>: Crystal clear water (ADC: 1800+)<br>
              &bull; <span style="color:#2ecc71">300-700</span>: Slightly cloudy<br>
              &bull; <span style="color:#e67e22">700-1300</span>: Cloudy water<br>
              &bull; <span style="color:#e74c3c">1300+</span>: Very turbid - Filter needed
            </div>
        </div>
        
        <div class="analysis-box">
            <div class="analysis-title">Nutrient Availability Analysis</div>
            <div class="analysis-item" id="nutrientStatus">
                Waiting for sensor data...
            </div>
        </div>
        
        <div class="controls">
            <button class="btn btn-refresh" onclick="fetchData()">Refresh Now</button>
            <button id="toggleBtn" class="btn btn-stop" onclick="toggleRefresh()">Stop Auto Refresh</button>
        </div>
        
        <div class="timestamp">
            Last updated: <span id="timestamp">--:--:--</span>
        </div>
    </div>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  // Initialize I2C LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  
  // Initialize pH sensor
  analogReadResolution(12);
  
  // Initialize DS18B20
  sensors.begin();
  
  delay(1000);
  
  // Display welcome message on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Water Quality");
  lcd.setCursor(0, 1);
  lcd.print("Monitor");
  
  delay(2000);
  
  // Connect to WiFi
  Serial.println("\nConnecting to WiFi...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connecting");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  
  WiFi.begin(ssid, password);
  
  // Set WiFi power to fix connection issues
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    
    // Show connection progress on LCD
    lcd.setCursor(15, 1);
    lcd.print(".");
    attempts++;
    
    if (attempts % 6 == 0) {
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(ssid);
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print("IP:");
    lcd.print(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Failed! Running in standalone mode.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed");
    lcd.setCursor(0, 1);
    lcd.print("Standalone Mode");
  }
  
  delay(2000);
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  
  // Clear LCD for normal operation
  lcd.clear();
  
  delay(1000);
}

void loop() {
  // Handle web server clients
  server.handleClient();
  
  // Read sensors every 2 seconds
  static unsigned long lastSensorRead = 0;
  if (millis() - lastSensorRead >= 2000) {
    readAllSensors();
    updateLCD();
    lastSensorRead = millis();
  }
}

void readAllSensors() {
  // Read pH sensor
  int buffer_arr[10];
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(PH_PIN);
    delay(30);
  }
  
  // Sort values
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        int temp_val = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp_val;
      }
    }
  }
  
  // Average middle 6 values
  unsigned long int avgval = 0;
  for (int i = 2; i < 8; i++) {
    avgval += buffer_arr[i];
  }
  
  // Convert ADC to voltage and calculate pH
  float voltage = (float)avgval * 3.3 / 4095.0 / 6.0;
  ph_value = -5.70 * voltage + ph_calibration;
  
  // Read temperature
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  
  // Read TDS
  int tds_raw = analogRead(TDS_PIN);
  if (tds_raw > 0) {
    float tds_voltage = tds_raw * 3.3 / 4095.0;
    tds_value = tds_voltage * tds_calibration * 1000;
  }
  
  // ===== TURBIDITY CALCULATION =====
  // Read raw ADC value
  turbidity_raw = analogRead(TURBIDITY_PIN);
  
  // Calculate voltage for display
  turbidity_voltage = turbidity_raw * 3.3 / 4095.0;
  
  // INVERSE CALCULATION:
  // Clear water: ADC > 1800 → show low turbidity (<300)
  // Dirty water: ADC < 1800 → show high turbidity (>1300)
  
  // Map ADC value inversely to turbidity scale:
  // ADC 4095 (max) → Turbidity 0 (minimum)
  // ADC 0 → Turbidity 2000 (maximum)
  // ADC 1800 (clear water threshold) → Turbidity ~1100
  
  // Simple inverse linear mapping
  turbidity_value = map(turbidity_raw, 4095, 0, 0, 2000);
  
  // Alternative: Two-stage mapping for better accuracy
  /*
  if (turbidity_raw >= 1800) {
    // Clear water range: 1800-4095 → 0-300
    turbidity_value = map(turbidity_raw, 4095, 1800, 0, 300);
  } else {
    // Dirty water range: 0-1800 → 300-2000
    turbidity_value = map(turbidity_raw, 1800, 0, 300, 2000);
  }
  */
  
  // Ensure turbidity value is within reasonable bounds
  if (turbidity_value < 0) turbidity_value = 0;
  if (turbidity_value > 2000) turbidity_value = 2000;
  
  // Print to Serial Monitor
  Serial.print("pH: ");
  Serial.print(ph_value, 2);
  Serial.print(" | Temp: ");
  Serial.print(temperature, 1);
  Serial.print("C | TDS: ");
  Serial.print(tds_value, 0);
  Serial.print(" ppm | Turbidity: ");
  Serial.print(turbidity_value, 0);
  Serial.print(" (Raw: ");
  Serial.print(turbidity_raw);
  Serial.print(" ADC, ");
  Serial.print(turbidity_voltage, 3);
  Serial.println("V)");
}

String getNutrientAnalysis() {
  String analysis = "";
  
  // pH-based nutrient availability
  if (ph_value >= 6.0 && ph_value <= 7.0) {
    analysis += "N:High ";
  } else if (ph_value < 6.0 || ph_value > 7.0) {
    analysis += "N:Low ";
  }
  
  if (ph_value >= 6.5 && ph_value <= 7.5) {
    analysis += "P:High ";
  } else {
    analysis += "P:Low ";
  }
  
  if (ph_value >= 6.0 && ph_value <= 7.5) {
    analysis += "K:High";
  } else {
    analysis += "K:Low";
  }
  
  // Temperature effects
  if (temperature < 15) {
    analysis += " | Uptake:Slow";
  } else if (temperature >= 20 && temperature <= 25) {
    analysis += " | Uptake:Optimal";
  } else if (temperature > 30) {
    analysis += " | Uptake:Reduced";
  }
  
  return analysis;
}

void updateLCD() {
  if (millis() - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    displayMode = (displayMode + 1) % 4;
    lastDisplayUpdate = millis();
    
    lcd.clear();
    
    switch (displayMode) {
      case 0:  // Display pH
        lcd.setCursor(0, 0);
        lcd.print("pH Value:");
        lcd.setCursor(0, 1);
        lcd.print(ph_value, 2);
        lcd.print(" pH");
        break;
        
      case 1:  // Display Temperature
        lcd.setCursor(0, 0);
        lcd.print("Temperature:");
        lcd.setCursor(0, 1);
        lcd.print(temperature, 1);
        lcd.print(" C");
        break;
        
      case 2:  // Display TDS
        lcd.setCursor(0, 0);
        lcd.print("TDS:");
        lcd.setCursor(0, 1);
        lcd.print(tds_value, 0);
        lcd.print(" ppm");
        break;
        
      case 3:  // Display Turbidity
        lcd.setCursor(0, 0);
        lcd.print("Turbidity:");
        lcd.setCursor(0, 1);
        lcd.print(turbidity_value, 0);
        lcd.print(" TU");
        break;
    }
  }
}

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleData() {
  String json = "{";
  json += "\"ph\":" + String(ph_value, 2) + ",";
  json += "\"temp\":" + String(temperature, 1) + ",";
  json += "\"tds\":" + String(tds_value, 0) + ",";
  json += "\"turbidity\":" + String(turbidity_value, 0) + ",";
  json += "\"turbidity_raw\":" + String(turbidity_raw) + ",";
  json += "\"turbidity_voltage\":" + String(turbidity_voltage, 3) + ",";
  json += "\"analysis\":\"" + getNutrientAnalysis() + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}