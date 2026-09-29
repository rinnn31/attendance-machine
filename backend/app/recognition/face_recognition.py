from dataclasses import dataclass

import insightface
import os
import re 

@dataclass(slots=True)
class FaceData:
    embedding: list[float]
    bounding_box: tuple[int, int, int, int]
    pose: tuple[float, float, float]
    score: float

class FaceRecognition:
    def __init__(self):
        self._model = insightface.app.FaceAnalysis(
            name='buffalo_l'
        )
        det_size = os.environ.get("FACE_IMAGE_DATA_SIZE", "640x640")
        if re.match(r'^\d+x\d+$', det_size):
            width, height = map(int, det_size.split('x'))
            self._model.prepare(
                ctx_id=0 if os.environ.get("USE_CUDA", "false").lower() == "true" else -1,
                det_size=(width, height)
            )
        else:
            raise ValueError("FACE_IMAGE_DATA_SIZE must be in the format 'widthxheight', e.g., '640x640'.")

        self._face_embeddings : dict[str, list[list[float]]] = {}

        pass

    def add_face_embedding(self, employee_id: str, embedding: list[float]):
        if employee_id not in self._face_embeddings:
            self._face_embeddings[employee_id] = []
        self._face_embeddings[employee_id].append(embedding)

    def extract_faces_data(self, image: bytes) -> list[FaceData]:
        faces = self._model.get(image)
        faces_data = []
        for face in faces:
            face_data = FaceData(
                embedding=face.embedding.tolist(),
                bounding_box=tuple(face.bbox.astype(int)),
                pose=tuple(face.pose),
                score=float(face.det_score)
            )
            faces_data.append(face_data)
        return faces_data

    def recognize_face(self, embedding: list[float], threshold: float = 0.5) -> str | None:
        best_match_id = None
        best_match_score = float('inf')

        for employee_id, embeddings in self._face_embeddings.items():
            for stored_embedding in embeddings:
                score = self._model.compute_distance(embedding, stored_embedding)
                if score < best_match_score and score < threshold:
                    best_match_score = score
                    best_match_id = employee_id

        return best_match_id