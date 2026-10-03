FROM python:3.12-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    libgl1 \
    libglib2.0-0 \
    libsm6 \
    libxext6 \
    libxrender-dev \
    libxcb1 \
    && rm -rf /var/lib/apt/lists/*
    
WORKDIR /app

COPY pyproject.toml ./

ARG USE_CUDA="false"

RUN pip install --upgrade pip setuptools wheel && \
    if [ "$USE_CUDA" = "true" ] || [ "$USE_CUDA" = "1" ] ; then \
        pip install --no-cache-dir .[gpu] ; \
    else \
        pip install --no-cache-dir .[cpu] ; \
    fi

COPY . .

EXPOSE 8000

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]