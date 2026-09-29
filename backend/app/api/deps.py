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