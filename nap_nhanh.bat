@echo off
chcp 65001 >nul
title NAP CODE ESP32-C3 SIEU TOC (5 GIAY)
cls
echo ======================================================
echo    BO CONG CU NAP NHANH ESP32-C3 KHONG CAN BIEN DICH
echo ======================================================
echo.

echo [1/3] Dang tai file firmware_app.bin tu Cloud...
curl -L -s https://raw.githubusercontent.com/khang26042012/ecoclean-bin/main/firmware_app.bin -o firmware_app.bin

if not exist firmware_app.bin (
    echo [LOI] Khong tai duoc firmware_app.bin! Hay kiem tra ket noi mang.
    pause
    exit /b
)

echo [2/3] Kiem tra esptool 32-bit...
if not exist esptool.exe (
    echo Dang tai esptool.exe 32-bit cho Win10...
    curl -L -s https://raw.githubusercontent.com/khang26042012/ecoclean-bin/raw/main/esptool.exe -o esptool.exe
)

echo [3/3] San sang nap!
echo.
echo Hay nhap cong COM cua ESP32 (vi du COM3, COM4, COM5):
set /p COMPORT=">> Cong COM (mac dinh COM5 neu bo trong): "
if "%COMPORT%"=="" set COMPORT=COM5

echo.
echo Dang nap firmware vao %COMPORT% o toc do 115200 (mat khoang 5 giay)...
echo.
esptool.exe --chip esp32c3 --port %COMPORT% --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x10000 firmware_app.bin

echo.
echo ======================================================
echo    DA XONG! ESP32-C3 SE KHOI DONG LAI VA BAT DEN XANH
echo ======================================================
echo.
pause
