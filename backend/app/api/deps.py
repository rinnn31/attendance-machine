from fastapi import Request

async def get_database(request: Request):
    database = request.app.state.database
    session = database.get_session()
    try:
        yield session
    finally:
        await session.close()

def get_device_manager(request: Request):
    return request.app.state.device_manager

def get_face_recognition(request: Request):
    return request.app.state.face_recognition

def get_fingerprint_recognition(request: Request):
    return request.app.state.fingerprint_recognition