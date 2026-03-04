import cv2
import time
import requests
import argparse
from ultralytics import YOLO

# ---------------- USER SETTINGS ----------------
ESP_IP = "10.28.229.224"   # Change this to your ESP32 IP
SEND_DELAY = 0.1          # Delay between HTTP requests (seconds)
CONF_THRESH = 0.3           # Confidence threshold for YOLO
# ----------------------------------------------

parser = argparse.ArgumentParser()
parser.add_argument('--model', required=True, help="Path to YOLO model file (e.g., my_model.pt)")
parser.add_argument('--source', required=True, help="Video file path or camera index (e.g., 0)")
args = parser.parse_args()

model_path = args.model
video_source = args.source

# Load YOLO model
model = YOLO(model_path)

# Open video or camera
try:
    video_source = int(video_source)  # If numeric, treat as camera index
except:
    pass
cap = cv2.VideoCapture(video_source)
if not cap.isOpened():
    print("ERROR: Cannot open video source")
    exit()

print("Starting YOLO + ESP32 Video Tracking")
print("Press 'q' to quit\n")

last_send_time = 0

while True:
    ret, frame = cap.read()
    if not ret:
        print("Video ended or cannot read frame")
        break

    # Run YOLO detection
    results = model(frame, verbose=False)
    detections = results[0].boxes

    best_conf = 0
    best_center = None

    # Find highest confidence object
    for i in range(len(detections)):
        conf = float(detections[i].conf.item())
        if conf > CONF_THRESH and conf > best_conf:
            xyxy = detections[i].xyxy.cpu().numpy().squeeze().astype(int)
            xmin, ymin, xmax, ymax = xyxy
            center_x = int((xmin + xmax) / 2)
            center_y = int((ymin + ymax) / 2)

            best_conf = conf
            best_center = (center_x, center_y, xmin, ymin, xmax, ymax)

    # If an object was detected, send coordinates to ESP32
    if best_center is not None:
        center_x, center_y, xmin, ymin, xmax, ymax = best_center

        # Draw bounding box and center
        cv2.rectangle(frame, (xmin, ymin), (xmax, ymax), (0, 255, 0), 2)
        cv2.circle(frame, (center_x, center_y), 5, (0, 0, 255), -1)
        cv2.putText(frame, f"X:{center_x} Y:{center_y}",
                    (center_x + 10, center_y - 10),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.6,
                    (255, 255, 255),
                    2)

        # Print coordinates in PC terminal
        print(f"X: {center_x}   Y: {center_y}")

        # Send to ESP32
        current_time = time.time()
        if current_time - last_send_time > SEND_DELAY:
            try:
                url = f"http://{ESP_IP}/?x={center_x}&y={center_y}"
                requests.get(url, timeout=1)
                last_send_time = current_time
            except:
                print("Warning: ESP32 not reachable")

    else:
        print("No object detected")

    # Show video frame
    cv2.imshow("YOLO + ESP32 Tracking", frame)

    # Quit on 'q' key
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# Cleanup
cap.release()
cv2.destroyAllWindows()
print("Tracking stopped.")