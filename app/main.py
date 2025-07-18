from fastapi import FastAPI, Request
from server import ToolBServer

# 1. Create a standard FastAPI application
app = FastAPI()

@app.get("/")
def read_root():
    return {"message": "Hello from FastAPI running on toolB!"}

@app.get("/api/users")
def get_user(id: int, role: str):
    return {"user_id": id, "user_role": role, "message": "Query params parsed successfully"}

@app.post("/api/data")
async def create_data(request: Request):
    json_body = await request.json()
    return {
        "message": "POST request successful",
        "received_data": json_body
    }

# 2. Run the app with the ToolBServer
if __name__ == "__main__":
    print("🚀 Launching FastAPI app with ToolBServer...")
    # Instead of uvicorn, we instantiate and run our custom server
    server = ToolBServer(app=app)
    server.run()
