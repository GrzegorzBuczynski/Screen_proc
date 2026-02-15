#include <windows.h>
#include <vector>
#include <cstdint>
#include <cstdio>   // tylko do przykładu wypisania

// Zwraca true jeśli udało się, dane w vectorze pixels (BGRA, wiersze od dołu!)
bool CaptureScreenToMemory(std::vector<uint8_t>& pixels,
                           int& width,
                           int& height)
{
    // GetSystemMetrics - pobiera wymiary ekranu w pikselach
    // SM_CXSCREEN = szerokość głównego monitora
    // SM_CYSCREEN = wysokość głównego monitora
    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);

    if (width <= 0 || height <= 0) return false;

    // GetDC - pobiera kontekst urządzenia (Device Context) dla całego ekranu
    // NULL = pobierz DC dla całego ekranu (a nie dla konkretnego okna)
    // DC jest potrzebne do operacji graficznych (rysowanie, kopiowanie)
    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC) return false;

    // CreateCompatibleDC - tworzy kontekst urządzenia w pamięci RAM
    // hScreenDC = tworzy DC zgodny z ekranem
    // Pamięciowy DC jest używany do operacji "off-screen" (bufora w pamięci)
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    if (!hMemDC) {
        ReleaseDC(NULL, hScreenDC);
        return false;
    }

    // CreateCompatibleBitmap - tworzy bitmapę zgodną z ekranem o określonym rozmiarze
    // hScreenDC = bazuje na formacie koloru ekranu
    // width, height = wymiary bitmapy w pikselach
    // Ta bitmapa będzie przechowywać skopiowaną zawartość ekranu
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    if (!hBitmap) {
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hScreenDC);
        return false;
    }

    // SelectObject - przypisuje bitmapę do pamięciowego DC
    // Od teraz wszystkie operacje rysowania na hMemDC będą zapisywane w hBitmap
    SelectObject(hMemDC, hBitmap);

    // BitBlt - kopiuje piksele z jednego DC do drugiego (bit block transfer)
    // hMemDC = DC docelowy (nasza bitmapa w pamięci)
    // 0, 0 = pozycja docelowa (lewy górny róg)
    // width, height = rozmiar obszaru do skopiowania
    // hScreenDC = DC źródłowy (ekran)
    // 0, 0 = pozycja źródłowa (lewy górny róg ekranu)
    // SRCCOPY = tryb kopiowania (zwykłe kopiowanie bez mieszania)
    BitBlt(hMemDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

    // Przygotowanie struktury do pobrania surowych pikseli
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;  // dane od góry
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // Rozmiar bufora na piksele (4 bajty na piksel)
    size_t byteCount = static_cast<size_t>(width) * height * 4;
    pixels.resize(byteCount);

    // GetDIBits - pobiera surowe dane pikseli z bitmapy do tablicy
    // hMemDC = kontekst urządzenia
    // hBitmap = bitmapa źródłowa
    // 0 = pierwszy wiersz do skopiowania
    // height = liczba wierszy do skopiowania
    // pixels.data() = wskaźnik do bufora docelowego
    // &bmi = informacje o formacie bitmapy (32-bit BGRA)
    // DIB_RGB_COLORS = użyj bezpośrednich wartości RGB (nie palety kolorów)
    // Zwraca liczbę skopiowanych wierszy
    int lines = GetDIBits(hMemDC,
                          hBitmap,
                          0,
                          height,
                          pixels.data(),
                          &bmi,
                          DIB_RGB_COLORS);

    bool success = (lines == height);

    // Sprzątanie - zwalnianie zasobów GDI (Graphics Device Interface)
    // DeleteObject - usuwa obiekt GDI (bitmapę) z pamięci
    DeleteObject(hBitmap);
    // DeleteDC - usuwa pamięciowy kontekst urządzenia
    DeleteDC(hMemDC);
    // ReleaseDC - zwalnia kontekst urządzenia ekranu (para dla GetDC)
    ReleaseDC(NULL, hScreenDC);

    return success;
}

void safeToFile(const std::vector<uint8_t>& pixels, int w, int h)
{
     FILE* file = fopen("screenshot.bmp", "wb");
            if (file) {
                // Nagłówek BMP
                BITMAPFILEHEADER fileHeader{};
                fileHeader.bfType = 0x4D42; // "BM"
                fileHeader.bfSize = 54 + (DWORD)pixels.size();
                fileHeader.bfOffBits = 54;

                BITMAPINFOHEADER infoHeader{};
                infoHeader.biSize = sizeof(BITMAPINFOHEADER);
                infoHeader.biWidth = w;
                infoHeader.biHeight = -h;  // dla zapisu BMP top-down (jeśli źródło było -height)
                infoHeader.biPlanes = 1;
                infoHeader.biBitCount = 32;
                infoHeader.biCompression = BI_RGB;

                fwrite(&fileHeader, sizeof(BITMAPFILEHEADER), 1, file);
                fwrite(&infoHeader, sizeof(BITMAPINFOHEADER), 1, file);
                fwrite(pixels.data(), pixels.size(), 1, file);
                fclose(file);
                printf("Screenshot zapisany do: screenshot.bmp\n");
            }
}

// ────────────────────────────────────────────────
// Przykład użycia
// ────────────────────────────────────────────────
int main()
{
    SetProcessDPIAware();
    std::vector<uint8_t> pixels;
    int w = 0, h = 0;

    if (CaptureScreenToMemory(pixels, w, h))
    {
        printf("Złapano ekran %d × %d → %zu bajtów\n", w, h, pixels.size());

        // Przykład: pierwszy piksel w lewym górnym rogu (BGRA!)
        if (!pixels.empty())
        {
            uint8_t b = pixels[0];
            uint8_t g = pixels[1];
            uint8_t r = pixels[2];
            uint8_t a = pixels[3];  // zwykle 255
            printf("Lewy górny piksel: R=%3d G=%3d B=%3d A=%3d\n", r,g,b,a);
            // Zapis do pliku BMP
            safeToFile(pixels, w, h);
        }
        

        // Tutaj możesz już pracować na tablicy pixels – np. analiza, wysyłanie przez sieć, OpenCV itd.
    }
    else
    {
        printf("Nie udało się złapać ekranu\n");
    }

    return 0;
}