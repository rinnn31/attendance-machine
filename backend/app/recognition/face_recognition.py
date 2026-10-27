from dataclasses import dataclass
import cv2
import insightface
import re
import numpy as np 

from app.core.config import get_settings

@dataclass(slots=True)
class FaceData:
    embedding: list[float]
    bounding_box: tuple[int, int, int, int]
    pose: tuple[float, float, float]
    score: float

class FaceRecognition:
    def __init__(self):
        use_cuda = get_settings().USE_CUDA

        self._model = insightface.app.FaceAnalysis(
            name='buffalo_l' if use_cuda else 'buffalo_s'
        )
        det_size = get_settings().FACE_IMAGE_DATA_SIZE
        if re.match(r'^\d+x\d+$', det_size):
            width, height = map(int, det_size.split('x'))
            self._model.prepare(
                ctx_id=0 if use_cuda else -1,
                det_size=(width, height)
            )
        else:
            raise ValueError("FACE_IMAGE_DATA_SIZE must be in the format 'widthxheight', e.g., '640x640'.")

        self._face_embeddings : dict[int, list[list[float]]] = {}

        pass

    def add_face_embedding(self, employee_id: int, embedding: list[float]):
        if employee_id not in self._face_embeddings:
            self._face_embeddings[employee_id] = []
        self._face_embeddings[employee_id].append(embedding)

    def remove_face_embeddings(self, employee_id: int):
        if employee_id in self._face_embeddings:
            del self._face_embeddings[employee_id]

    def extract_faces_data(self, image: bytes) -> list[FaceData]:
        nparr = np.frombuffer(image, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        
        faces = self._model.get(img)
        faces_data = []
        for face in faces:
            print(f"Detected face with score {face.det_score}, bounding box {face.bbox}, and pose {face.pose}")
            face_data = FaceData(
                embedding=face.embedding.tolist(),
                bounding_box=tuple(face.bbox.astype(int)),
                pose=tuple(face.pose),
                score=float(face.det_score)
            )
            faces_data.append(face_data)
        return faces_data

    def recognize_face(self, embedding: list[float], threshold: float = 0.5) -> int | None:
        query_vec = np.array(embedding, dtype=np.float32)
        query_norm = np.linalg.norm(query_vec)
        if query_norm == 0:
            return None
        query_vec /= query_norm

        best_match_id = None
        best_score = -1.0

        for employee_id, embeddings in self._face_embeddings.items():
            matrix = np.array(embeddings, dtype=np.float32)
            norms = np.linalg.norm(matrix, axis=1, keepdims=True)
            matrix = np.divide(matrix, norms, out=matrix, where=norms != 0)

            similarities = np.dot(matrix, query_vec)
            max_sim = float(np.max(similarities))

            print(f"Comparing with employee_id {employee_id}, max similarity: {max_sim}")

            if max_sim > best_score and max_sim >= threshold:
                best_score = max_sim
                best_match_id = employee_id

        return best_match_id