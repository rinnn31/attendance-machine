import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image, ImageTk
from afis import LeaderExtractor
import cv2

class FingerprintApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Phần Mềm Nhận Diện Vân Tay - SourceAFIS")
        self.root.geometry("650x540")
        self.root.config(bg="#f4f6f9")
        self.root.resizable(False, False)

        self.path1 = None
        self.path2 = None

        self.create_widgets()

    def create_widgets(self):
        # Tiêu đề ứng dụng
        title_label = tk.Label(
            self.root, 
            text="HỆ THỐNG SO KHỚP VÂN TAY (SOURCEAFIS)", 
            font=("Arial", 14, "bold"), 
            bg="#f4f6f9", 
            fg="#2c3e50"
        )
        title_label.pack(pady=15)

        # Khung chứa 2 ảnh vân tay
        frame_images = tk.Frame(self.root, bg="#f4f6f9")
        frame_images.pack(pady=5)

        # Ô chọn Ảnh 1 (Probe)
        frame1 = tk.LabelFrame(frame_images, text=" Ảnh Vân Tay Gốc (Probe) ", font=("Arial", 10, "bold"), bg="#f4f6f9")
        frame1.grid(row=0, column=0, padx=15)
        
        self.lbl_img1 = tk.Label(frame1, text="Chưa chọn ảnh", width=22, height=11, bg="white", relief="groove")
        self.lbl_img1.pack(padx=10, pady=10)
        
        btn_browse1 = tk.Button(frame1, text="Chọn Ảnh 1", command=lambda: self.load_image(1), bg="#2ecc71", fg="white", font=("Arial", 9, "bold"), cursor="hand2")
        btn_browse1.pack(pady=5)

        # Ô chọn Ảnh 2 (Candidate)
        frame2 = tk.LabelFrame(frame_images, text=" Ảnh So Sánh (Candidate) ", font=("Arial", 10, "bold"), bg="#f4f6f9")
        frame2.grid(row=0, column=1, padx=15)
        
        self.lbl_img2 = tk.Label(frame2, text="Chưa chọn ảnh", width=22, height=11, bg="white", relief="groove")
        self.lbl_img2.pack(padx=10, pady=10)
        
        btn_browse2 = tk.Button(frame2, text="Chọn Ảnh 2", command=lambda: self.load_image(2), bg="#3498db", fg="white", font=("Arial", 9, "bold"), cursor="hand2")
        btn_browse2.pack(pady=5)

        # Nút thực hiện so khớp
        self.btn_match = tk.Button(
            self.root, 
            text="TIẾN HÀNH SO KHỚP", 
            command=self.match_fingerprints, 
            bg="#e67e22", 
            fg="white", 
            font=("Arial", 11, "bold"), 
            width=22, 
            height=2,
            cursor="hand2"
        )
        self.btn_match.pack(pady=20)

        # Nhãn hiển thị kết quả
        self.lbl_result = tk.Label(
            self.root, 
            text="Kết quả: Vui lòng chọn 2 ảnh và bấm so khớp", 
            font=("Arial", 11, "bold"), 
            bg="#f4f6f9", 
            fg="#7f8c8d"
        )
        self.lbl_result.pack(pady=5)

    def load_image(self, img_num):
        """Mở hộp thoại chọn file ảnh và hiển thị lên giao diện"""
        file_path = filedialog.askopenfilename(
            title=f"Chọn ảnh vân tay {img_num}", 
            filetypes=[("Image files", "*.jpg;*.jpeg;*.png;*.bmp")]
        )
        if file_path:
            if img_num == 1:
                self.path1 = file_path
                self.display_image(file_path, self.lbl_img1)
            else:
                self.path2 = file_path
                self.display_image(file_path, self.lbl_img2)

    def display_image(self, path, widget):
        """Xử lý resize và hiển thị ảnh lên Label của Tkinter"""
        try:
            img = Image.open(path)
            img = img.resize((140, 140), Image.Resampling.LANCZOS)
            img_tk = ImageTk.PhotoImage(img)
            widget.config(image=img_tk, text="")
            widget.image = img_tk  # Giữ tham chiếu để tránh bị Garbage Collection thu hồi
        except Exception as e:
            messagebox.showerror("Lỗi", f"Không thể tải ảnh: {e}")

    def match_fingerprints(self):
        """Thực hiện so khớp bằng SourceAFIS và hiển thị điểm số"""
        if not self.path1 or not self.path2:
            messagebox.showwarning("Cảnh báo", "Vui lòng chọn đầy đủ cả 2 ảnh vân tay trước khi so khớp!")
            return

        try:
            self.lbl_result.config(text="Đang xử lý trích xuất và so khớp...", fg="blue")
            self.root.update()

            import time


            source_extractor = LeaderExtractor()
            t1 = time.perf_counter()
            img = cv2.imread(self.path1, cv2.IMREAD_GRAYSCALE)
            
            template1 = source_extractor.extract_minutiae(img)
            template2 = source_extractor.extract_minutiae(self.path2)
        
            t2 = time.perf_counter()
            print(f"Thời gian trích xuất: {t2 - t1:.4f} giây")

            score = source_extractor.match(template1, template2, threshold=0.5)
            print(score.raw_score)
            # Ngưỡng chuẩn của SourceAFIS: Điểm >= 40 được coi là trùng khớp an toàn
            THRESHOLD = 40.0
            
            if score.score >= THRESHOLD:
                result_text = f"🎯 TRÙNG KHỚP! (Điểm số: {score.score:.2f})"
                color = "#27ae60"  # Màu xanh lá
            else:
                result_text = f"❌ KHÔNG TRÙNG KHỚP! (Điểm số: {score.score:.2f})"
                color = "#c0392b"  # Màu đỏ

            self.lbl_result.config(text=result_text, fg=color)

        except Exception as e:
            messagebox.showerror("Lỗi", f"Đã xảy ra lỗi trong quá trình so khớp:\n{e}")
            self.lbl_result.config(text="Kết quả: Lỗi xử lý", fg="red")

if __name__ == "__main__":
    root = tk.Tk()
    app = FingerprintApp(root)
    root.mainloop()