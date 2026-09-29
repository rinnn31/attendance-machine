from dataclasses import dataclass
import cv2
import numpy as np

@dataclass(slots=True)
class FingerprintData:
    template: np.ndarray 
    score: float

class FingerprintRecognition:
    def __init__(self):
        self._model = cv2.SIFT_create()
        self._fingerprint_templates: dict[str, list[np.ndarray]] = {}

    def _preprocess_image(self, image_bytes: bytes) -> np.ndarray:
        np_arr = np.frombuffer(image_bytes, np.uint8)
        img = cv2.imdecode(np_arr, cv2.IMREAD_GRAYSCALE)
        
        if img is None:
            raise ValueError("Không thể decode ảnh vân tay đầu vào.")

        # Cân bằng sáng cục bộ để tăng độ tương phản (CLAHE)
        clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
        enhanced = clahe.apply(img)

        # Làm mờ ảnh để giảm nhiễu
        blurred = cv2.GaussianBlur(enhanced, (3, 3), 0)
            
        return blurred

    def add_fingerprint_template(self, employee_id: str, template: np.ndarray):
        if employee_id not in self._fingerprint_templates:
            self._fingerprint_templates[employee_id] = []
        self._fingerprint_templates[employee_id].append(template)

    def extract_fingerprint_data(self, image: bytes) -> FingerprintData | None:
        try:
            preprocessed_img = self._preprocess_image(image)
            
            keypoints, descriptors = self._model.detectAndCompute(preprocessed_img, None)
            
            if descriptors is not None:
                fp_data = FingerprintData(
                    template=descriptors,
                    score=1.0
                )
                return fp_data
            else:
                return None
                
        except Exception as e:
            return None
            

    def recognize_fingerprint(self, template: np.ndarray, threshold: int = 15) -> str | None:
        best_match_id = None
        max_good_matches = 0

        matcher = cv2.FlannBasedMatcher(dict(algorithm=1, trees=5), dict(checks=50))

        for employee_id, stored_templates in self._fingerprint_templates.items():
            for stored_template in stored_templates:
                if stored_template is None or len(stored_template) < 2 or len(template) < 2:
                    continue
                
                try:
                    matches = matcher.knnMatch(template, stored_template, k=2)
                    
                    good_matches = 0
                    for match_points in matches:
                        if len(match_points) == 2:
                            m, n = match_points
                            # Khoảng cách điểm m phải nhỏ hơn 70% khoảng cách điểm n
                            if m.distance < 0.7 * n.distance:
                                good_matches += 1

                    if good_matches > max_good_matches and good_matches >= threshold:
                        max_good_matches = good_matches
                        best_match_id = employee_id

                except Exception as e:
                    pass

        return best_match_id