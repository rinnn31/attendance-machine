import cv2
import math
import numpy as np
import onnxruntime as ort
from insightface.app import FaceAnalysis

import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image, ImageTk

def check_looking_at_camera(pose, bbox, image_shape, fov_degrees=65.0, pitch_thresh=15.0, yaw_thresh=15.0):
    if pose is None or len(pose) < 3 or bbox is None:
        return False, None
        
    pitch, yaw, roll = pose
    img_h, img_w = image_shape[:2]
    
    face_x = (bbox[0] + bbox[2]) / 2.0
    face_y = (bbox[1] + bbox[3]) / 2.0
    
    img_center_x = img_w / 2.0
    img_center_y = img_h / 2.0
    
    fov_rad = math.radians(fov_degrees)
    focal_length = (img_w / 2.0) / math.tan(fov_rad / 2.0)
    
    expected_yaw = math.degrees(math.atan((face_x - img_center_x) / focal_length))
    expected_pitch = math.degrees(math.atan((face_y - img_center_y) / focal_length))
    
    expected_yaw *= -1 
    expected_pitch *= -1 
    
    yaw_diff = abs(yaw - expected_yaw)
    pitch_diff = abs(pitch - expected_pitch)
    
    is_looking = (pitch_diff <= pitch_thresh) and (yaw_diff <= yaw_thresh)
    
    info = {
        'yaw': yaw,
        'pitch': pitch,
        'yaw_diff': yaw_diff,
        'pitch_diff': pitch_diff
    }
    
    return is_looking, info

def get_primary_face(faces, img_shape, center_threshold=0.3):
    if not faces:
        return None
        
    img_h, img_w = img_shape[:2]
    img_cx, img_cy = img_w / 2.0, img_h / 2.0
    max_radius = img_w * center_threshold 

    valid_faces = []
    
    for face in faces:
        bbox = face.bbox
        face_cx = (bbox[0] + bbox[2]) / 2.0
        face_cy = (bbox[1] + bbox[3]) / 2.0
        
        distance = math.sqrt((face_cx - img_cx)**2 + (face_cy - img_cy)**2)
        
        if distance <= max_radius:
            area = (bbox[2] - bbox[0]) * (bbox[3] - bbox[1])
            valid_faces.append({'face': face, 'area': area})
            
    if not valid_faces:
        return None
        
    valid_faces.sort(key=lambda x: x['area'], reverse=True)
    return valid_faces[0]['face']

def setup_insightface():
    providers = ort.get_available_providers()
    
    if 'CUDAExecutionProvider' in providers:
        print("Phát hiện CUDA. Sử dụng GPU cho nhận diện khuôn mặt.")
        ctx_id = 0
        model_name = 'buffalo_l' 
        det_size = (640, 640)
    else:
        print("Không phát hiện CUDA. Sử dụng CPU cho nhận diện khuôn mặt.")
        ctx_id = -1
        model_name = 'buffalo_s' 
        det_size = (320, 320)
        
    app = FaceAnalysis(name=model_name)
    app.prepare(ctx_id=ctx_id, det_size=det_size)
    
    return app

class FaceTestApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Face Dectection Test")
        self.root.geometry("900x700")
        
        self.app = setup_insightface()
        
        self.cap = None
        self.is_streaming = False
        
        btn_frame = tk.Frame(root)
        btn_frame.pack(pady=10)
        
        self.btn_upload = tk.Button(btn_frame, text="Tải ảnh lên", command=self.upload_image, width=15, font=("Arial", 10, "bold"))
        self.btn_upload.pack(side=tk.LEFT, padx=10)
        
        self.btn_camera = tk.Button(btn_frame, text="Bật Camera", command=self.toggle_camera, width=15, font=("Arial", 10, "bold"))
        self.btn_camera.pack(side=tk.LEFT, padx=10)
        
        self.lbl_video = tk.Label(root, bg="black")
        self.lbl_video.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)

    def process_and_draw(self, frame, is_live=False):
        if is_live:
            frame = cv2.flip(frame, 1)
            
        img_h, img_w = frame.shape[:2]
        
        center_radius = int(img_w * 0.3)
        cv2.circle(frame, (int(img_w/2), int(img_h/2)), center_radius, (255, 255, 255), 1, cv2.LINE_AA)
        
        faces = self.app.get(frame)
        target_face = get_primary_face(faces, frame.shape, center_threshold=0.3)
        
        if target_face is not None:
            bbox = target_face.bbox.astype(int)
            pose = target_face.pose
            
            is_looking, info = check_looking_at_camera(pose, bbox, frame.shape)
            color = (0, 255, 0) if is_looking else (0, 165, 255) 
            
            cv2.rectangle(frame, (bbox[0], bbox[1]), (bbox[2], bbox[3]), color, 2)
            
            if info is not None:
                status = "HOP LE" if is_looking else "SAI TU THE"
                cv2.putText(frame, status, (bbox[0], bbox[1] - 10), 
                            cv2.FONT_HERSHEY_SIMPLEX, 0.8, color, 2)
                
                y_offset = bbox[3] + 25
                cv2.putText(frame, f"Yaw lech: {info['yaw_diff']:.1f}", (bbox[0], y_offset), 
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 1)
                cv2.putText(frame, f"Pitch lech: {info['pitch_diff']:.1f}", (bbox[0], y_offset + 20), 
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 1)
                
        return frame

    def display_on_tkinter(self, frame):
        rgb_image = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        pil_img = Image.fromarray(rgb_image)
        
        max_width, max_height = 800, 600
        if pil_img.width > max_width or pil_img.height > max_height:
            pil_img.thumbnail((max_width, max_height), Image.Resampling.LANCZOS)
            
        imgtk = ImageTk.PhotoImage(image=pil_img)
        self.lbl_video.imgtk = imgtk
        self.lbl_video.configure(image=imgtk)

    def upload_image(self):
        self.stop_camera()
        
        file_path = filedialog.askopenfilename(
            title="Chọn ảnh",
            filetypes=[("Image files", "*.jpg *.jpeg *.png")]
        )
        
        if file_path:
            frame = cv2.imread(file_path)
            if frame is not None:
                processed_frame = self.process_and_draw(frame, is_live=False)
                self.display_on_tkinter(processed_frame)
            else:
                messagebox.showerror("Lỗi", "Không thể đọc ảnh này!")

    def toggle_camera(self):
        if self.is_streaming:
            self.stop_camera()
        else:
            self.start_camera()

    def start_camera(self):
        self.cap = cv2.VideoCapture(0)
        if not self.cap.isOpened():
            messagebox.showerror("Lỗi", "Không thể kết nối với webcam!")
            return
            
        self.is_streaming = True
        self.btn_camera.config(text="Tắt Camera", fg="red")
        self.update_camera_frame()

    def stop_camera(self):
        self.is_streaming = False
        self.btn_camera.config(text="Bật Camera", fg="black")
        if self.cap:
            self.cap.release()
            self.cap = None

    def update_camera_frame(self):
        if self.is_streaming and self.cap:
            ret, frame = self.cap.read()
            if ret:
                processed_frame = self.process_and_draw(frame, is_live=True)
                self.display_on_tkinter(processed_frame)
            
            self.root.after(15, self.update_camera_frame)

    def on_closing(self):
        self.stop_camera()
        self.root.destroy()

if __name__ == '__main__':
    root = tk.Tk()
    app = FaceTestApp(root)
    root.mainloop()