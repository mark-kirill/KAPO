import string

def nibble_xor_encrypt(text, key_nibble):
    """4-битное XOR шифрование"""
    if isinstance(text, str):
        text = text.encode()
    
    # Применяем 4-битный XOR к каждому байту
    encrypted = []
    for b in text:
        # Берем только младшие 4 бита ключа
        key_4bit = key_nibble & 0x0F
        # Применяем XOR к каждому полубайту
        encrypted_byte = ((b & 0xF0) ^ (key_4bit << 4)) | ((b & 0x0F) ^ key_4bit)
        encrypted.append(encrypted_byte)
    
    return bytes(encrypted)

def nibble_xor_decrypt(cipher_bytes, key_nibble):
    """4-битная XOR дешифровка (симметричная)"""
    return nibble_xor_encrypt(cipher_bytes, key_nibble)

def break_nibble_xor(cipher_hex):
    """Взлом 4-битного XOR"""
    cipher_bytes = bytes.fromhex(cipher_hex)
    
    print("🔓 ВЗЛОМ 4-БИТНОГО XOR")
    print(f"🔐 Зашифрованный текст: {cipher_hex}")
    print(f"📊 Длина: {len(cipher_bytes)} байт")
    
    found_solutions = []
    
    # Перебираем только 16 возможных 4-битных ключей (0-15)
    for key in range(16):
        decrypted = nibble_xor_decrypt(cipher_bytes, key)
        
        try:
            decrypted_text = decrypted.decode('ascii')
            # Проверяем, что все символы печатные
            if all(c in (string.ascii_letters + string.digits + string.punctuation + ' ') for c in decrypted_text):
                found_solutions.append((decrypted_text, key))
                print(f"   🔑 Ключ 0x{key:01X}: '{decrypted_text}'")
        except UnicodeDecodeError:
            # Пропускаем не-ASCII текст
            pass
    
    print(f"\n🎯 Найдено {len(found_solutions)} возможных решений из 16 ключей")
    return found_solutions

def interactive_nibble_mode():
    """Интерактивный режим для 4-битного XOR"""
    print("🚀 ИНТЕРАКТИВНЫЙ РЕЖИМ 4-БИТНЫЙ XOR")
    print("=" * 50)
    
    # 1. Получаем исходные данные
    target_password = input("Введите искомое слово (8 символов): ").strip()
    if len(target_password) != 8:
        print("❌ Слово должно быть длиной 8 символов!")
        return
    
    key_input = input("Введите 4-битный XOR ключ (0-15 или 0x0-0xF): ").strip()
    if key_input.startswith('0x'):
        xor_key = int(key_input, 16)
    else:
        xor_key = int(key_input)
    
    # Ограничиваем 4 битами
    xor_key = xor_key & 0x0F
    
    print(f"\n🔐 РЕЗУЛЬТАТ ШИФРОВАНИЯ:")
    print(f"   Исходное слово: {target_password}")
    print(f"   4-битный XOR ключ: 0x{xor_key:01X} ({xor_key})")
    print(f"   Всего возможных ключей: 16")
    
    # 2. Шифруем слово
    cipher = nibble_xor_encrypt(target_password, xor_key)
    cipher_hex = cipher.hex()
    
    print(f"   Зашифрованная строка (hex): {cipher_hex}")
    print(f"   Байты шифра: {[hex(b) for b in cipher]}")
    
    # 3. Спрашиваем что известно для взлома
    print(f"\n🔍 ЧТО ИЗВЕСТНО ДЛЯ ВЗЛОМА?")
    print("1. Известен ключ и полный шифр")
    print("2. Известен только шифр (неизвестен ключ)")
    print("3. Известен шифр и часть пароля")
    
    choice = input("Выберите вариант (1-3): ").strip()
    
    if choice == "1":
        print(f"\n🎯 ВАРИАНТ 1: Известен ключ 0x{xor_key:01X} и шифр {cipher_hex}")
        print("🔓 МГНОВЕННАЯ ДЕШИФРОВКА...")
        result = nibble_xor_decrypt(cipher, xor_key).decode()
        print(f"✅ Расшифрованный текст: {result}")
        
        if result == target_password:
            print("🎉 УСПЕХ! Пароль корректно расшифрован")
        else:
            print("❌ Ошибка в дешифровке")
            
    elif choice == "2":
        print(f"\n🎯 ВАРИАНТ 2: Известен только шифр {cipher_hex}")
        print("🔓 ПЕРЕБОР 16 ВОЗМОЖНЫХ КЛЮЧЕЙ...")
        
        solutions = break_nibble_xor(cipher_hex)
        
        # Ищем правильный пароль среди решений
        correct_solution = None
        for text, key in solutions:
            if text == target_password:
                correct_solution = (text, key)
                break
        
        if correct_solution:
            print(f"\n🎉 НАЙДЕН ПРАВИЛЬНЫЙ ПАРОЛЬ!")
            print(f"   Ключ: 0x{correct_solution[1]:01X}")
            print(f"   Текст: '{correct_solution[0]}'")
        else:
            print(f"\n⚠️  Правильный пароль не найден среди возможных решений")
            
    elif choice == "3":
        print(f"\n🎯 ВАРИАНТ 3: Известен шифр {cipher_hex} и часть пароля")
        
        known_positions = {}
        known_chars = {}
        
        while True:
            pos = input("Введите позицию известного символа (0-7) или 'done': ").strip()
            if pos.lower() == 'done':
                break
            try:
                pos = int(pos)
                if pos < 0 or pos > 7:
                    print("❌ Позиция должна быть от 0 до 7")
                    continue
                char = input(f"Введите символ на позиции {pos}: ").strip()
                if len(char) != 1:
                    print("❌ Введите один символ")
                    continue
                known_positions[pos] = char
                known_chars[pos] = char
                print(f"✅ Добавлен символ '{char}' на позиции {pos}")
            except ValueError:
                print("❌ Введите число от 0 до 7")
        
        if not known_positions:
            print("❌ Не указано ни одного известного символа")
            return
        
        print(f"\n🔍 ПОИСК КЛЮЧА ПО ИЗВЕСТНЫМ СИМВОЛАМ...")
        cipher_bytes = bytes.fromhex(cipher_hex)
        
        possible_keys = []
        for key in range(16):  # Все 16 возможных ключей
            try:
                decrypted = nibble_xor_decrypt(cipher_bytes, key).decode('ascii')
                
                # Проверяем длину строки
                if len(decrypted) == 8:
                    # ПРАВИЛЬНАЯ ПРОВЕРКА СОВПАДЕНИЙ
                    all_matches = True
                    for pos, char in known_chars.items():
                        if pos >= len(decrypted) or decrypted[pos] != char:
                            all_matches = False
                            break
                    
                    if all_matches:
                        possible_keys.append((decrypted, key))
                        print(f"   🔑 Ключ 0x{key:01X}: '{decrypted}' ✓")
            except UnicodeDecodeError:
                # Пропускаем не-ASCII текст
                continue
        
        if possible_keys:
            if len(possible_keys) == 1:
                result, found_key = possible_keys[0]
                print(f"\n🎉 ОДНОЗНАЧНО ОПРЕДЕЛЕН КЛЮЧ: 0x{found_key:01X}")
                print(f"✅ Расшифрованный текст: {result}")
                
                if result == target_password:
                    print("🎉 УСПЕХ! Пароль корректно взломан")
                else:
                    print("⚠️  Найден пароль, но он не соответствует исходному")
            else:
                print(f"\n⚠️  Найдено {len(possible_keys)} возможных ключей:")
                for i, (text, key) in enumerate(possible_keys, 1):
                    status = "🎯 ПРАВИЛЬНЫЙ" if text == target_password else "возможный"
                    print(f"   {i}. Ключ 0x{key:01X}: '{text}' ({status})")
        else:
            print("❌ Не найдено ключей, удовлетворяющих известным символам")
    
    else:
        print("❌ Неверный выбор")

# Демонстрация работы 4-битного XOR
def demo_nibble_xor():
    """Демонстрация 4-битного XOR"""
    print("\n" + "="*50)
    print("🔬 ДЕМОНСТРАЦИЯ 4-БИТНОГО XOR")
    print("="*50)
    
    test_text = "KAPOTeam"
    test_key = 0x5  # 4-битный ключ
    
    print(f"Исходный текст: '{test_text}'")
    print(f"4-битный ключ: 0x{test_key:01X}")
    
    # Шифрование
    encrypted = nibble_xor_encrypt(test_text, test_key)
    print(f"Зашифрованный (hex): {encrypted.hex()}")
    
    # Дешифровка
    decrypted = nibble_xor_decrypt(encrypted, test_key).decode()
    print(f"Дешифрованный: '{decrypted}'")
    
    # Показываем все возможные варианты
    print(f"\nВсе 16 возможных дешифровок:")
    for key in range(16):
        text = nibble_xor_decrypt(encrypted, key).decode('ascii', errors='ignore')
        print(f"  Ключ 0x{key:01X}: '{text}'")

# Запуск
if __name__ == "__main__":
    demo_nibble_xor()
    print("\n")
    interactive_nibble_mode()