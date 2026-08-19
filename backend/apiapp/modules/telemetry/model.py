"""No Beanie Document by design.

Phase 1 telemetry is in-memory only -- the latest sample lives in
``apiapp/infrastructure/telemetry_hub.py`` and is streamed straight to clients, so there
is nothing to persist and no collection to create.

This file exists to keep the enforced 5-file module layout intact and to give Phase 2's
``FlameEvent`` document an obvious home. Note that
``apiapp/infrastructure/database.py::_gather_documents()`` auto-registers every
``beanie.Document`` subclass found in ``modules/*/model.py``, so adding a Document here
creates its collection automatically -- and adding one prematurely would create a
collection Phase 1 must not have.

Precedent for a module without a model: ``apiapp/modules/health/``.
"""
