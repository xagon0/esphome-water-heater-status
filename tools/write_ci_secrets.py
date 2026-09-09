"""Write deliberately fake compile-only credentials; never overwrite user secrets."""
import base64
from pathlib import Path

target = Path(__file__).resolve().parents[1] / "secrets.yaml"
dummy_key = base64.b64encode(bytes(range(32))).decode("ascii")
content = (
    '# DUMMY BUILD VALUES — DO NOT FLASH THIS CONFIGURATION\n'
    'wifi_ssid: "ci-only-not-a-real-network"\n'
    'wifi_password: "ci-only-not-a-real-password"\n'
    f'burner_camera_api_key: "{dummy_key}"\n'
    'burner_camera_ota_password: "ci-only-not-for-deployment"\n'
)
try:
    with target.open("x") as handle:
        handle.write(content)
except FileExistsError:
    raise SystemExit("secrets.yaml already exists; leave it unchanged.")
target.chmod(0o600)
print("Created ignored, dummy secrets.yaml for compilation only.")
