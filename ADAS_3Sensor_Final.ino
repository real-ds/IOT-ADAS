#include <WiFi.h>
#include <WebServer.h>

// ============================================================
// 3-SENSOR ADAS SYSTEM FOR LOW-VISIBILITY ENVIRONMENTS
// Project: IoT Fundamentals (ECE-3501) - Group 12
// Purpose: Forward collision detection & driver assistance
// ============================================================

// --- SENSOR CONFIGURATION (3 HC-SR04 ULTRASONIC SENSORS) ---
// Left Sensor - monitors left lane (30° angle from center)
#define TRIG_PIN_L 19
#define ECHO_PIN_L 21

// Center Sensor - monitors center lane (direct forward)
#define TRIG_PIN_C 5
#define ECHO_PIN_C 18

// Right Sensor - monitors right lane (30° angle from center)
#define TRIG_PIN_R 22
#define ECHO_PIN_R 23

// --- THREAT LEVEL CLASSIFICATION THRESHOLDS (per project specs) ---
// Table 2: Threat Level Classification System
#define UNSAFE_THRESHOLD 10.0      // < 10 cm   → RED   (Immediate braking required)
#define CAREFUL_THRESHOLD 40.0     // 10-40 cm  → YELLOW (Reduce speed, prepare to stop)
#define SAFE_THRESHOLD 100.0       // > 40 cm   → GREEN (Normal driving)

// --- SYSTEM CONFIGURATION ---
#define SENSOR_TIMEOUT_US 23200    // ~4 meters max range
#define UPDATE_INTERVAL_MS 750     // Update webpage every 750ms
#define MEASUREMENTS_PER_SENSOR 3  // Average 3 readings per sensor for stability

// --- WEB SERVER SETUP ---
WebServer server(80);

// ============================================================
// GLOBAL VARIABLES FOR SENSOR DATA
// ============================================================
float distLeft = 999.0;
float distCenter = 999.0;
float distRight = 999.0;

// ============================================================
// FUNCTION: getDistance()
// Purpose: Measure distance using ultrasonic sensor
// Returns: Distance in cm, or 999.0 if no valid reading
// Algorithm: Time-of-flight distance = (duration × speed_of_sound) / 2
// ============================================================
float getDistance(int trigPin, int echoPin) {
  // Configure pins for this sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  // Send 10 microsecond pulse to trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Wait for echo pulse and measure its duration
  // Speed of sound: ~343 cm/s at room temperature
  // Formula: distance = (time_in_microseconds × 0.0343) / 2
  long duration = pulseIn(echoPin, HIGH, SENSOR_TIMEOUT_US);
  
  if (duration == 0) {
    return 999.0; // Return max value if no echo detected
  }
  
  float distance = duration * 0.0343 / 2;
  
  // Filter out invalid readings (noise or reflections)
  if (distance < 2.0 || distance > 400.0) {
    return 999.0;
  }
  
  return distance;
}

// ============================================================
// FUNCTION: getMeasurementWithAveraging()
// Purpose: Take multiple readings and return the median
// This reduces sensor noise and improves stability
// ============================================================
float getMeasurementWithAveraging(int trigPin, int echoPin) {
  float readings[MEASUREMENTS_PER_SENSOR];
  
  // Collect multiple readings
  for (int i = 0; i < MEASUREMENTS_PER_SENSOR; i++) {
    readings[i] = getDistance(trigPin, echoPin);
    delay(10); // Small delay between measurements
  }
  
  // Simple sorting for median calculation
  for (int i = 0; i < MEASUREMENTS_PER_SENSOR - 1; i++) {
    for (int j = i + 1; j < MEASUREMENTS_PER_SENSOR; j++) {
      if (readings[i] > readings[j]) {
        float temp = readings[i];
        readings[i] = readings[j];
        readings[j] = temp;
      }
    }
  }
  
  // Return median value (middle value)
  return readings[MEASUREMENTS_PER_SENSOR / 2];
}

// ============================================================
// FUNCTION: getThreatLevel()
// Purpose: Classify threat level based on distance
// Returns: Threat code as string (for JSON response)
// ============================================================
String getThreatLevel(float distance) {
  if (distance < UNSAFE_THRESHOLD) {
    return "unsafe";
  } else if (distance < CAREFUL_THRESHOLD) {
    return "careful";
  } else if (distance <= SAFE_THRESHOLD) {
    return "safe";
  } else {
    return "clear";
  }
}

// ============================================================
// FUNCTION: handleRoot()
// Purpose: Serve the main visualization webpage
// Features:
//   - Responsive design for mobile & infotainment systems
//   - Real-time threat visualization with 3-lane display
//   - High-contrast colors for quick recognition
//   - Zone-based distance display
// ============================================================
void handleRoot() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <meta charset="UTF-8">
      <title>ADAS Threat Map - 3 Sensor System</title>
      <style>
        * {
          margin: 0;
          padding: 0;
          box-sizing: border-box;
        }
        
        body {
          background: linear-gradient(135deg, #0a0e27 0%, #1a1f3a 100%);
          color: #ffffff;
          font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
          overflow: hidden;
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          min-height: 100vh;
          padding: 10px;
        }
        
        .header {
          text-align: center;
          margin-bottom: 20px;
          z-index: 20;
        }
        
        .header h1 {
          font-size: 1.8em;
          font-weight: 700;
          margin-bottom: 5px;
          letter-spacing: 1px;
        }
        
        .header p {
          font-size: 0.9em;
          opacity: 0.8;
          color: #a0aec0;
        }
        
        #road-container {
          position: relative;
          width: 100%;
          max-width: 500px;
          height: 70vh;
          background: linear-gradient(to bottom, #1a2f4f 0%, #0f1a2e 100%);
          border-radius: 15px;
          overflow: hidden;
          box-shadow: 0 20px 60px rgba(0, 0, 0, 0.8), inset 0 0 30px rgba(10, 200, 255, 0.1);
          border: 2px solid #0a8fcc;
          margin-bottom: 20px;
        }
        
        /* Animated lane markings */
        .lane-marker {
          position: absolute;
          width: 100%;
          height: 100%;
          top: 0;
          pointer-events: none;
        }
        
        .lane-line {
          position: absolute;
          height: 100%;
          width: 2px;
          background-image: linear-gradient(to bottom, #4a5f7f 50%, transparent 50%);
          background-size: 100% 40px;
          animation: scroll 10s linear infinite;
        }
        
        .lane-line:nth-child(1) {
          left: 33.33%;
        }
        
        .lane-line:nth-child(2) {
          left: 66.66%;
        }
        
        @keyframes scroll {
          0% { background-position: 0 0; }
          100% { background-position: 0 40px; }
        }
        
        /* Vehicle representation */
        #vehicle {
          position: absolute;
          bottom: 30px;
          left: 50%;
          transform: translateX(-50%);
          width: 60px;
          height: 100px;
          background: linear-gradient(to bottom, #0af 0%, #06f 50%, #004080 100%);
          border-radius: 12px;
          border: 3px solid #ffffff;
          box-shadow: 0 0 30px rgba(0, 200, 255, 0.8), inset -2px -2px 5px rgba(0, 0, 0, 0.4);
          z-index: 100;
        }
        
        #vehicle::before {
          content: '';
          position: absolute;
          top: 8px;
          left: 6px;
          width: 48px;
          height: 25px;
          background: linear-gradient(135deg, #1a1a2e 0%, #2d2d4f 100%);
          border-radius: 8px;
          border: 2px solid #666;
        }
        
        /* Three-lane zone display */
        .zones-container {
          position: absolute;
          top: 0;
          left: 0;
          width: 100%;
          height: 100%;
          display: flex;
          pointer-events: none;
        }
        
        .zone {
          flex: 1;
          position: relative;
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: flex-start;
          padding-top: 20px;
        }
        
        .object-indicator {
          width: 45px;
          height: 45px;
          border-radius: 50%;
          display: none;
          position: absolute;
          top: 100px;
          transition: top 0.2s ease-out, background 0.2s, box-shadow 0.2s;
          z-index: 10;
        }
        
        .object-indicator.visible {
          display: flex;
          align-items: center;
          justify-content: center;
          font-size: 12px;
          font-weight: bold;
          color: #000;
        }
        
        /* Threat level colors */
        .object-indicator.unsafe {
          background: #ff1744;
          box-shadow: 0 0 25px #ff1744, 0 0 50px rgba(255, 23, 68, 0.5);
          animation: pulse-red 0.5s infinite;
        }
        
        .object-indicator.careful {
          background: #ffeb3b;
          box-shadow: 0 0 20px #ffeb3b, 0 0 40px rgba(255, 235, 59, 0.4);
          animation: pulse-yellow 0.8s infinite;
        }
        
        .object-indicator.safe {
          background: #00e676;
          box-shadow: 0 0 15px #00e676, 0 0 30px rgba(0, 230, 118, 0.3);
        }
        
        @keyframes pulse-red {
          0%, 100% { transform: scale(1); }
          50% { transform: scale(1.1); }
        }
        
        @keyframes pulse-yellow {
          0%, 100% { transform: scale(1); }
          50% { transform: scale(1.05); }
        }
        
        /* Status panel */
        .status-panel {
          width: 100%;
          max-width: 500px;
          padding: 15px 20px;
          background: rgba(10, 20, 40, 0.9);
          border: 2px solid #0a8fcc;
          border-radius: 10px;
          text-align: center;
          margin-bottom: 15px;
          z-index: 20;
        }
        
        .status-text {
          font-size: 1.1em;
          font-weight: 600;
          letter-spacing: 0.5px;
        }
        
        .status-text.clear {
          color: #00e676;
        }
        
        .status-text.safe {
          color: #00e676;
        }
        
        .status-text.careful {
          color: #ffeb3b;
        }
        
        .status-text.unsafe {
          color: #ff1744;
        }
        
        .distance-info {
          font-size: 0.85em;
          opacity: 0.8;
          margin-top: 8px;
          color: #a0aec0;
        }
        
        /* Data display panel */
        .data-panel {
          width: 100%;
          max-width: 500px;
          display: grid;
          grid-template-columns: 1fr 1fr 1fr;
          gap: 10px;
          padding: 15px;
          background: rgba(10, 20, 40, 0.9);
          border: 2px solid #0a8fcc;
          border-radius: 10px;
          z-index: 20;
        }
        
        .lane-data {
          text-align: center;
          padding: 12px;
          background: rgba(10, 150, 200, 0.2);
          border-radius: 8px;
          border-left: 3px solid #0a8fcc;
        }
        
        .lane-label {
          font-size: 0.9em;
          font-weight: 700;
          margin-bottom: 5px;
          text-transform: uppercase;
          letter-spacing: 1px;
        }
        
        .lane-distance {
          font-size: 1.3em;
          font-weight: 700;
          font-family: 'Courier New', monospace;
          color: #0af;
        }
        
        .lane-status {
          font-size: 0.75em;
          margin-top: 4px;
          opacity: 0.9;
        }
        
        /* Responsive design */
        @media (max-height: 600px) {
          .header { margin-bottom: 10px; }
          #road-container { height: 50vh; }
          .header h1 { font-size: 1.4em; }
        }
      </style>
    </head>
    <body>
      <div class="header">
        <h1>🚗 ADAS Threat Map</h1>
        <p>3-Sensor Forward Collision Detection System</p>
      </div>
      
      <div id="road-container">
        <div class="lane-marker">
          <div class="lane-line"></div>
          <div class="lane-line"></div>
        </div>
        
        <div class="zones-container">
          <div class="zone">
            <div id="obj-left" class="object-indicator"></div>
          </div>
          <div class="zone">
            <div id="obj-center" class="object-indicator"></div>
          </div>
          <div class="zone">
            <div id="obj-right" class="object-indicator"></div>
          </div>
        </div>
        
        <div id="vehicle"></div>
      </div>
      
      <div class="status-panel">
        <div id="status" class="status-text clear">● Scanning...</div>
        <div id="distance-info" class="distance-info"></div>
      </div>
      
      <div class="data-panel">
        <div class="lane-data">
          <div class="lane-label">LEFT</div>
          <div id="dist-left" class="lane-distance">---</div>
          <div id="status-left" class="lane-status">Unknown</div>
        </div>
        <div class="lane-data">
          <div class="lane-label">CENTER</div>
          <div id="dist-center" class="lane-distance">---</div>
          <div id="status-center" class="lane-status">Unknown</div>
        </div>
        <div class="lane-data">
          <div class="lane-label">RIGHT</div>
          <div id="dist-right" class="lane-distance">---</div>
          <div id="status-right" class="lane-status">Unknown</div>
        </div>
      </div>

      <script>
        const UNSAFE_THRESHOLD = 10;
        const CAREFUL_THRESHOLD = 40;
        const SAFE_THRESHOLD = 100;
        const ROAD_HEIGHT = document.getElementById('road-container').offsetHeight;
        const VEHICLE_TOP = ROAD_HEIGHT - 130;
        
        // DOM elements
        const objLeft = document.getElementById('obj-left');
        const objCenter = document.getElementById('obj-center');
        const objRight = document.getElementById('obj-right');
        const statusDisplay = document.getElementById('status');
        const distanceInfo = document.getElementById('distance-info');
        
        const distLeftDisplay = document.getElementById('dist-left');
        const distCenterDisplay = document.getElementById('dist-center');
        const distRightDisplay = document.getElementById('dist-right');
        
        const statusLeftDisplay = document.getElementById('status-left');
        const statusCenterDisplay = document.getElementById('status-center');
        const statusRightDisplay = document.getElementById('status-right');
        
        /**
         * Update visual indicator for a single zone
         */
        function updateZone(objElement, distance, statusElement) {
          if (distance >= SAFE_THRESHOLD || isNaN(distance)) {
            objElement.classList.remove('visible', 'unsafe', 'careful', 'safe');
            if (statusElement) statusElement.textContent = 'Clear';
            return null;
          }
          
          objElement.classList.add('visible');
          
          let threatLevel = 'safe';
          let threatText = 'Safe';
          
          if (distance < UNSAFE_THRESHOLD) {
            threatLevel = 'unsafe';
            threatText = '🚨 UNSAFE';
          } else if (distance < CAREFUL_THRESHOLD) {
            threatLevel = 'careful';
            threatText = '⚠️ CAREFUL';
          }
          
          objElement.classList.remove('unsafe', 'careful', 'safe');
          objElement.classList.add(threatLevel);
          
          // Position indicator based on distance
          const maxDistance = SAFE_THRESHOLD;
          const yOffset = (distance / maxDistance) * (VEHICLE_TOP * 0.9);
          const y = Math.max(20, VEHICLE_TOP - yOffset);
          objElement.style.top = y + 'px';
          
          if (statusElement) statusElement.textContent = threatText;
          return threatLevel;
        }
        
        /**
         * Determine overall system status based on closest threat
         */
        function getOverallStatus(dL, dC, dR) {
          const distances = [dL, dC, dR];
          const minDistance = Math.min(...distances);
          
          if (minDistance >= SAFE_THRESHOLD) {
            return { level: 'clear', text: '✓ SYSTEM CLEAR - All zones safe' };
          }
          
          let zone = 'Unknown';
          if (minDistance === dL) zone = 'Left';
          else if (minDistance === dC) zone = 'Center';
          else zone = 'Right';
          
          if (minDistance < UNSAFE_THRESHOLD) {
            return {
              level: 'unsafe',
              text: `🚨 DANGER - ${zone} lane (${minDistance.toFixed(0)} cm)`
            };
          } else if (minDistance < CAREFUL_THRESHOLD) {
            return {
              level: 'careful',
              text: `⚠️ CAUTION - ${zone} lane (${minDistance.toFixed(0)} cm)`
            };
          } else {
            return {
              level: 'safe',
              text: `● SAFE - ${zone} lane (${minDistance.toFixed(0)} cm)`
            };
          }
        }
        
        /**
         * Main update loop - fetch sensor data and update display
         */
        async function update() {
          try {
            const response = await fetch('/sensor-data');
            const data = await response.json();
            
            const dL = parseFloat(data.distL) || 999;
            const dC = parseFloat(data.distC) || 999;
            const dR = parseFloat(data.distR) || 999;
            
            // Update visual zones
            updateZone(objLeft, dL, statusLeftDisplay);
            updateZone(objCenter, dC, statusCenterDisplay);
            updateZone(objRight, dR, statusRightDisplay);
            
            // Update distance displays
            distLeftDisplay.textContent = dL < 999 ? dL.toFixed(1) + ' cm' : '---';
            distCenterDisplay.textContent = dC < 999 ? dC.toFixed(1) + ' cm' : '---';
            distRightDisplay.textContent = dR < 999 ? dR.toFixed(1) + ' cm' : '---';
            
            // Update overall status
            const status = getOverallStatus(dL, dC, dR);
            statusDisplay.textContent = status.text;
            statusDisplay.className = 'status-text ' + status.level;
            
            // Update distance info
            const validDistances = [dL, dC, dR].filter(d => d < 999);
            if (validDistances.length > 0) {
              const minDist = Math.min(...validDistances);
              distanceInfo.textContent = `Closest object: ${minDist.toFixed(0)} cm`;
            } else {
              distanceInfo.textContent = 'No objects detected';
            }
            
          } catch (error) {
            console.error('Connection error:', error);
            statusDisplay.textContent = '✗ System Error - No connection';
            statusDisplay.className = 'status-text';
          }
        }
        
        // Update every 750ms as per project specification
        setInterval(update, 750);
        update(); // Initial update
      </script>
    </body>
    </html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

// ============================================================
// FUNCTION: handleSensorData()
// Purpose: JSON endpoint that returns all three sensor readings
// Response: {"distL": <float>, "distC": <float>, "distR": <float>}
// ============================================================
void handleSensorData() {
  // Read all three sensors with averaging
  float distL = getMeasurementWithAveraging(TRIG_PIN_L, ECHO_PIN_L);
  delay(15);
  float distC = getMeasurementWithAveraging(TRIG_PIN_C, ECHO_PIN_C);
  delay(15);
  float distR = getMeasurementWithAveraging(TRIG_PIN_R, ECHO_PIN_R);
  
  // Store for debugging/logging if needed
  distLeft = distL;
  distCenter = distC;
  distRight = distR;
  
  // Format JSON response
  String json = "{\"distL\": " + String(distL, 1) +
                ", \"distC\": " + String(distC, 1) +
                ", \"distR\": " + String(distR, 1) + "}";
  
  server.send(200, "application/json", json);
}

// ============================================================
// FUNCTION: handleNotFound()
// Purpose: Return 404 for invalid endpoints
// ============================================================
void handleNotFound() {
  server.send(404, "text/plain", "Endpoint not found");
}

// ============================================================
// SETUP FUNCTION
// Purpose: Initialize system, WiFi, and web server
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("=========================================");
  Serial.println("  3-SENSOR ADAS SYSTEM INITIALIZING");
  Serial.println("  Project: IoT Fundamentals (ECE-3501)");
  Serial.println("=========================================");
  
  // Create WiFi hotspot
  WiFi.softAP("ESP32-ADAS-3Sensor", "12345678");
  IPAddress ip = WiFi.softAPIP();
  
  Serial.println("WiFi Hotspot Created:");
  Serial.print("  SSID: ESP32-ADAS-3Sensor");
  Serial.print("  Password: 12345678");
  Serial.print("  IP Address: ");
  Serial.println(ip);
  Serial.println("  → Connect to hotspot and visit: http://192.168.4.1");
  
  // Configure web server routes
  server.on("/", handleRoot);
  server.on("/sensor-data", handleSensorData);
  server.onNotFound(handleNotFound);
  
  // Start web server
  server.begin();
  Serial.println("\n✓ Web server started");
  
  // Verify sensor pins are available
  Serial.println("\nSensor Configuration:");
  Serial.println("  Left Sensor   → Trigger: GPIO" + String(TRIG_PIN_L) + ", Echo: GPIO" + String(ECHO_PIN_L));
  Serial.println("  Center Sensor → Trigger: GPIO" + String(TRIG_PIN_C) + ", Echo: GPIO" + String(ECHO_PIN_C));
  Serial.println("  Right Sensor  → Trigger: GPIO" + String(TRIG_PIN_R) + ", Echo: GPIO" + String(ECHO_PIN_R));
  
  Serial.println("\nThreat Level Thresholds:");
  Serial.println("  UNSAFE:  < " + String(UNSAFE_THRESHOLD, 0) + " cm  (RED)");
  Serial.println("  CAREFUL: " + String(UNSAFE_THRESHOLD, 0) + "-" + String((int)CAREFUL_THRESHOLD) + " cm (YELLOW)");
  Serial.println("  SAFE:    > " + String((int)CAREFUL_THRESHOLD) + " cm  (GREEN)");
  
  Serial.println("\n✓ System ready");
  Serial.println("=========================================\n");
}

// ============================================================
// MAIN LOOP
// Purpose: Handle incoming HTTP requests
// ============================================================
void loop() {
  server.handleClient();
  
  // Optional: Print debug info every 5 seconds
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    lastDebug = millis();
    Serial.print("Distances [L/C/R]: ");
    Serial.print(distLeft); Serial.print("/");
    Serial.print(distCenter); Serial.print("/");
    Serial.println(distRight);
  }
}
