# 🛑 Co można, a czego lepiej unikać (Dos & Don'ts)

Bawisz się ESP32? Super! Zanim jednak puścisz wszystko z dymem, rzuć okiem na tę krótką ściągę:

## ✅ Śmiało, rób to (DOs):
- **Eksperymentuj z kodem!** Popsujesz? Zawsze możesz pobrać czystą wersję z repozytorium.
- **Używaj zasilania po USB** do wgrywania kodu. To najbezpieczniejsza opcja.
- **Sprawdzaj monitor portu szeregowego (Serial Monitor).** Jak coś nie działa, tam zazwyczaj ESP32 krzyczy, co go boli (pamiętaj o dobrym baud rate, np. 115200).
- **Zadawaj pytania / zgłaszaj issues.** Jeśli wpadłeś na fajny pomysł modyfikacji, daj znać!

## ❌ Lepiej tego nie rób (DON'Ts):
- **Nie podpinaj 5V pod piny 3.3V!** ESP32 tego nie zniesie i odda swoje krzemowe życie. Jeśli moduł działa na 5V, używaj konwertera poziomów logicznych (Level Shifter).
- **Nie wyciągaj kabla podczas wgrywania kodu.** Czasem to nic nie zepsuje, ale po co kusić los i uszkodzić bootloader?
- **Uważaj na pobór prądu.** Piny ESP32 dają radę z LED-em, ale nie podpinaj do nich bezpośrednio dużych silników czy grzałek! Użyj tranzystora albo przekaźnika.
- **Nie zostawiaj sprzętu na deszczu...** (chyba że dobrze go zalejesz żywicą albo zamkniesz w szczelnej puszce!).
