from collections.abc import AsyncGenerator

import pytest
from apiapp.core.config import Settings, get_settings
from apiapp.run import create_app
from asgi_lifespan import LifespanManager
from fastapi import FastAPI
from httpx import ASGITransport, AsyncClient
from pymongo import AsyncMongoClient


@pytest.fixture(scope="session")
def event_loop():
    """
    Create an instance of the default event loop for the session.
    """
    import asyncio

    loop = asyncio.new_event_loop()
    yield loop
    loop.close()


@pytest.fixture(scope="session")
def settings() -> Settings:
    return get_settings()


@pytest.fixture(scope="session")
async def db_client(settings: Settings) -> AsyncGenerator[AsyncMongoClient | None, None]:
    if not settings.DATABASE_URI:
        yield None
        return
    client = AsyncMongoClient(settings.DATABASE_URI, serverSelectionTimeoutMS=2000)
    yield client
    await client.close()


@pytest.fixture(scope="session")
async def app() -> AsyncGenerator[FastAPI, None]:
    """
    Create a FastAPI application instance for the test session.
    We use LifespanManager to ensure startup/shutdown events run.
    """
    _app = create_app()
    async with LifespanManager(_app):
        yield _app


@pytest.fixture(scope="function")
async def client(app: FastAPI) -> AsyncGenerator[AsyncClient, None]:
    """
    Create a test client for the FastAPI application.
    """
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as ac:
        yield ac


@pytest.fixture(scope="function", autouse=True)
async def clean_db(settings: Settings, db_client: AsyncMongoClient | None):
    if not settings.DATABASE_URI or db_client is None:
        yield
        return

    db_name = db_client.get_default_database().name
    if "test" not in db_name and "test" not in settings.APP_ENV:
        pytest.skip("Running against a non-test database! Aborting.")

    db = db_client[db_name]
    collections = await db.list_collection_names()
    for collection in collections:
        await db[collection].delete_many({})

    yield
