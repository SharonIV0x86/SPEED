# setup.py
from setuptools import setup, find_packages

setup(
    name="speed-ipc",
    version="1.0.0",
    author="SPEED Development Team",
    description="Secure Process-To-Process Encrypted Exchange and Delivery",
    packages=find_packages(),
    python_requires=">=3.7",
    install_requires=[
        "pynacl>=1.5.0",
        "filelock>=3.12.0",
    ],
)