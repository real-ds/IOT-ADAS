Low-Cost IoT-Based Driver Assistance System for Low-Visibility Environments
===========================================================================

About This Project
------------------

This repository contains the project files for a **Low-Cost IoT-Based Driver Assistance System**, created for the ECE-3501: IOT FUNDAMENTALS course.

The main goal of this project is to improve driver safety in low-visibility conditions like fog, heavy rain, dust storms, or at night. Many budget-friendly vehicles don't have advanced safety systems, and this project presents an affordable and practical solution to help drivers "see" obstacles they might otherwise miss.

What It Does
------------

The system acts as a simple, reliable warning aid. It uses sensors to detect obstructions in front of the vehicle and warns the driver through a simple visual map on their smartphone.

### How It Works

1.  **Sensing:** Three ultrasonic sensors are placed on the front of the vehicle to monitor three zones: **Left**, **Center**, and **Right**. Together, they cover a 120-degree area in front of the car.
    
2.  **Processing:** A small microcontroller (an ESP32) reads the data from these sensors in real-time.
    
3.  **Warning:** The system figures out how close an obstacle is and classifies the danger level:
    
    *   **Green:** Safe (obstacle is far away)
        
    *   **Yellow:** Careful (obstacle is getting close)
        
    *   **Red:** Unsafe (obstacle is very close, brake immediately)
        
4.  **Display:** The ESP32 creates its own local Wi-Fi hotspot (like a personal router, no internet needed). The driver just connects their phone or tablet to this Wi-Fi.
    
5.  **Visualization:** On their phone's web browser, the driver sees a simple "threat map" showing the three zones and their color-coded danger level. This map updates constantly as the car moves.
    

### Key Features

*   **Low-Cost:** Built with affordable, easy-to-find components.
    
*   **Self-Contained:** Requires **no internet connection** to work, making it reliable anywhere.
    
*   **All-Weather Operation:** Unlike cameras, ultrasonic sensors work well in fog, rain, dust, and darkness.
    
*   **Simple & Intuitive:** The color-coded map is easy to understand at a glance, minimizing driver distraction.
    

Project Contributors
--------------------

*   Divyanshu Singh (22MIS1150)
    
*   Krishna Yadav (22MIS1142)
    
*   K. Venkata Rama Krishna (22MIS1167)
