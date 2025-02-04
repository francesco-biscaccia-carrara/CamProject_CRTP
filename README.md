# 📸 CamProject_CRTP

CamProject_CRTP is a powerful yet lightweight **client-server application** designed to **capture, transmit, and store** webcam video frames seamlessly. With optional **MJPEG to MP4 conversion**, it's perfect for real-time streaming and recording!

---

## 🚀 Features
✨ **Webcam Capture** – Leverages V4L2 for MJPEG frame acquisition.    
🎥 **Network Transmission** – Sends frames efficiently over **TCP/IP**.     
💾 **Storage & Conversion** – Saves MJPEG files and optionally converts them to **MP4** using FFmpeg.

---

## 🛠️ Installation
Ensure you have a **Debian-based OS** and install dependencies:
```bash
sudo apt update && sudo apt install -y build-essential libsdl2-dev libsdl2-image-dev libv4l-dev v4l-utils ffmpeg
```
Clone the repository and compile:
```bash
git clone https://github.com/francesco-biscaccia-carrara/CamProject_CRTP.git
cd CamProject_CRTP
make
```

---

## 🎯 Usage
### 🖥️ Start the Server
```bash
./CServer <port> [-c]  # Use -c for automatic MJPEG to MP4 conversion
```
Example:
```bash
./CServer 8080 -c
```

### 📡 Start the Client
```bash
./CClient <port> <num_frames>  # Use -1 for continuous capture
```
Example:
```bash
./CClient 8080 100
```

---

## 📂 Project Structure
📁 `cam_client.c` – Client-side application.    
📁 `cam_server.c` – Server-side application.    
📁 `ext_lib/` – External dependencies.  
📄 `Makefile` – Build automation.   

---

## 📜 License
🔓 **MIT License** – See `LICENSE` for details.

Enjoy seamless webcam streaming with CamProject_CRTP! 🚀🎥

