from functools import lru_cache

from pydantic_settings import BaseSettings, SettingsConfigDict

class Settings(BaseSettings):
    DATABASE_URL: str
    AUTHORIZATION_TOKEN: str
    FACE_IMAGE_DATA_SIZE: str = "640x480"
    FINGERPRINT_IMAGE_DATA_SIZE: str = "128x128"
    USE_CUDA: bool = False
    FACE_CAPTURE_IMAGE_PATH: str = "face_captures/"

    model_config = SettingsConfigDict(
        env_file=".env", 
        env_file_encoding="utf-8",
        case_sensitive=True
    )

@lru_cache()
def get_settings() -> Settings:
    return Settings()