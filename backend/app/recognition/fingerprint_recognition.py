from dataclasses import dataclass
import cv2
import numpy as np
from afis import LeaderExtractor, FingerprintTemplate

class FingerprintRecognition:
    def __init__(self):
        self._model = LeaderExtractor()
        self._fingerprint_templates: dict[int, list[FingerprintTemplate]] = {}


    def add_fingerprint_template(self, employee_id: int, template: bytes):
        if employee_id not in self._fingerprint_templates:
            self._fingerprint_templates[employee_id] = []

        self._fingerprint_templates[employee_id].append(FingerprintTemplate.from_iso19794_2(template))

    def remove_fingerprint_templates(self, employee_id: int):
        if employee_id in self._fingerprint_templates:
            del self._fingerprint_templates[employee_id]

    def extract_fingerprint_data(self, image: bytes) -> FingerprintTemplate | None:
        try:
            print("Extracting fingerprint data from image with size:", len(image), "bytes")
            np_arr = np.frombuffer(image, np.uint8)
            img = cv2.imdecode(np_arr, cv2.IMREAD_GRAYSCALE)
            
            if img is None:
                raise ValueError("Không thể decode ảnh vân tay đầu vào.")
            
            template = self._model.extract_minutiae(img)
            return template
                
        except Exception as e:
            return None
            

    def recognize_fingerprint(self, image_bytes: bytes, threshold: int = 0.25) -> int | None:
        template = self.extract_fingerprint_data(image_bytes)
        if template is None:
            return None


        best_match_id = None
        max_score = 0

        for employee_id, stored_templates in self._fingerprint_templates.items():
            for stored_template in stored_templates:
                try:
                    result = self._model.match(template, stored_template)
                    score = result.raw_score
                    print(f"Matching with employee_id {employee_id}: score = {score}")
                    if score > max_score and score >= threshold:
                        max_score = score
                        best_match_id = employee_id

                except Exception:
                    pass

        return best_match_id