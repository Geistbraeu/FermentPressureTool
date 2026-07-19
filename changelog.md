# Changelog

## 1.2.2 - 2026-07-19

### Fixed
- OLED header now shows the configured device name (`devName`) from settings instead of the compile-time default hostname.
- mDNS now starts with the configured device name, so access via `http://<devName>.local/` works as expected after boot.
- When `devName` is changed in web settings, mDNS is restarted immediately and HTTP service is re-registered, so the new `.local` name applies without reflashing.
