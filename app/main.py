from fastapi import FastAPI, Request
from server import ToolBServer
import time
import multiprocessing
import configparser
from typing import Optional

# Create a standard FastAPI application
app = FastAPI()

@app.get("/")
def read_root():
    return {"message": "Hello reload!"}

@app.get("/api/users")
def get_user(id: int, role: Optional[str] = None):
    return {"user_id": id, "user_role": role, "message": "Query params parsed successfully"}

@app.post("/api/data")
async def create_data(request: Request):
    json_body = await request.json()
    return {
        "message": "POST request successful",
        "received_data": json_body
    }

# Run the app with the ToolBServer
if __name__ == "__main__":
    # --- Hot Reloading Flag ---
    # Set this to True for development to enable automatic restarts on code changes.
    # Set to False for production.
    RELOAD_ENABLED = True

    # "spawn" is the safest, most portable start method
    multiprocessing.set_start_method("spawn", force=True)

    config = configparser.ConfigParser()
    config.read('toolB.conf')

    print("🚀 Launching FastAPI app with ToolBServer...")
    server = ToolBServer(app_path="main:app", config=config)
    server.run(reload=RELOAD_ENABLED)
