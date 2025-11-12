import cupy as cp
import string
import time
import numpy as np

def calculate_batch_size(total_combinations, chars_count=62, password_length=8):
    """Рассчитывает оптимальный размер батча на основе доступной памяти GPU"""
    
    password_size_bytes = 8
    
    try:
        mem_info = cp.cuda.runtime.memGetInfo()
        gpu_free_memory = mem_info[0]
        available_memory = gpu_free_memory * 0.7
        
        print(f"🎯 Информация о памяти GPU:")
        print(f"   Свободно: {gpu_free_memory / (1024**3):.1f} GB")
        print(f"   Доступно для батча: {available_memory / (1024**3):.1f} GB")
        
    except:
        available_memory = 1 * 1024**3
        print(f"⚠️  Не удалось получить информацию о GPU памяти, используем {available_memory / (1024**3):.1f} GB")
    
    max_batch_elements = int(available_memory // password_size_bytes)
    max_batch_elements = min(max_batch_elements, 100_000_000)
    min_batch_elements = 1_000_000
    
    batch_size = max(min_batch_elements, max_batch_elements)
    
    print(f"📊 Расчет батча:")
    print(f"   Всего комбинаций: {total_combinations:,}")
    print(f"   Размер батча: {batch_size:,} паролей")
    
    return batch_size

def decrypt_with_known_key(cipher_hex, xor_key):
    """Мгновенная дешифровка при известном ключе и шифре"""
    cipher_bytes = bytes.fromhex(cipher_hex)
    decrypted = bytes([b ^ xor_key for b in cipher_bytes])
    return decrypted.decode()

def xor_encrypt(text, key):
    """XOR шифрование с однобайтовым ключом"""
    if isinstance(text, str):
        text = text.encode()
    return bytes([b ^ key for b in text])


def brute_force_with_known_parts(cipher_hex, known_positions, known_chars, xor_key=None):
    """Brute force с известными частями пароля"""
    print("🎯 BRUTE FORCE С ИЗВЕСТНЫМИ СИМВОЛАМИ...")
    
    if xor_key is None:
        # Вычисляем ключ по первому известному символу
        first_pos = min(known_positions.keys())
        xor_key = find_key_from_known_char(cipher_hex, known_chars[first_pos], first_pos)
        if xor_key is None:
            return None
    
    # Расширенный набор символов
    all_chars = (
        string.digits +
        string.ascii_letters + 
        string.punctuation
    )
    
    # Преобразуем зашифрованную строку
    cipher_bytes = bytes.fromhex(cipher_hex)
    
    # Вычисляем количество неизвестных позиций
    unknown_positions = [i for i in range(8) if i not in known_positions]
    total_unknown = len(unknown_positions)
    
    if total_unknown == 0:
        # Все символы известны - просто проверяем
        password = ''.join(known_chars.get(i, '?') for i in range(8))
        decrypted = xor_encrypt(password, xor_key)
        if decrypted == cipher_bytes:
            print(f"✅ Пароль найден (все символы известны): {password}")
            return password
        else:
            print("❌ Известные символы не соответствуют шифру")
            return None
    
    total_combinations = len(all_chars) ** total_unknown
    print(f"🔤 Неизвестных позиций: {total_unknown}")
    print(f"📈 Всего комбинаций для перебора: {total_combinations:,}")
    
    # Расчет размера батча
    batch_size = calculate_batch_size(total_combinations)
    
    start_time = time.time()
    ascii_codes = cp.array([ord(c) for c in all_chars], dtype=cp.uint64)
    
    try:
        found_password = None
        
        for batch_start in range(0, total_combinations, batch_size):
            if found_password:
                break
                
            batch_end = min(batch_start + batch_size, total_combinations)
            
            print(f"\n🔄 Батч {batch_start // batch_size + 1}: индексы {batch_start:,} - {batch_end:,}")
            
            # Создаем индексы для неизвестных позиций
            indices_gpu = cp.arange(batch_start, batch_end, dtype=cp.uint64)
            
            # Генерируем комбинации для неизвестных позиций
            unknown_chars = {}
            for i, pos in enumerate(unknown_positions):
                if i == len(unknown_positions) - 1:
                    idx = indices_gpu % len(all_chars)
                else:
                    power = len(all_chars) ** (len(unknown_positions) - i - 1)
                    idx = (indices_gpu // power) % len(all_chars)
                unknown_chars[pos] = ascii_codes[idx]
            
            # Собираем полные пароли
            passwords_gpu = cp.zeros_like(indices_gpu, dtype=cp.uint64)
            for pos in range(8):
                if pos in known_positions:
                    char_code = cp.uint64(ord(known_chars[pos]))
                else:
                    char_code = unknown_chars[pos]
                passwords_gpu |= (char_code << (8 * (7 - pos)))
            
            # Применяем XOR и сравниваем
            xor_key_64bit = cp.uint64(xor_key * 0x0101010101010101)
            encrypted_passwords = passwords_gpu ^ xor_key_64bit
            
            # Создаем целевое значение для сравнения
            target_numeric = 0
            for i, encrypted_char in enumerate(cipher_bytes):
                target_numeric |= (encrypted_char << (8 * (7 - i)))
            target_gpu = cp.uint64(target_numeric)
            
            matches = cp.where(encrypted_passwords == target_gpu)[0]
            
            if len(matches) > 0:
                found_idx_in_batch = int(matches[0])
                found_idx_global = batch_start + found_idx_in_batch
                
                # Восстанавливаем пароль
                found_val = int(passwords_gpu[found_idx_in_batch])
                found_chars = [
                    chr((found_val >> 56) & 0xFF),
                    chr((found_val >> 48) & 0xFF),
                    chr((found_val >> 40) & 0xFF),
                    chr((found_val >> 32) & 0xFF),
                    chr((found_val >> 24) & 0xFF),
                    chr((found_val >> 16) & 0xFF),
                    chr((found_val >> 8) & 0xFF),
                    chr(found_val & 0xFF)
                ]
                found_password = ''.join(found_chars)
                
                total_time = time.time() - start_time
                
                print(f"✅ ПАРОЛЬ НАЙДЕН: '{found_password}'")
                print(f"   Глобальный индекс: {found_idx_global:,}")
                print(f"   Общее время: {total_time:.3f}с")
                print(f"   Скорость: {batch_end/total_time:,.0f} комб/сек")
                break
            else:
                elapsed = time.time() - start_time
                progress = (batch_end / total_combinations) * 100
                speed = batch_end / elapsed if elapsed > 0 else 0
                
                print(f"   Прогресс: {progress:.2f}%")
                print(f"   Скорость: {speed:,.0f} комб/сек")
        
        if not found_password:
            total_time = time.time() - start_time
            print(f"\n❌ Пароль не найден")
            print(f"   Проверено: {min(total_combinations, batch_end):,} комбинаций")
            
        return found_password
        
    except Exception as e:
        print(f"❌ Ошибка: {e}")
        import traceback
        traceback.print_exc()
        return None

def full_brute_force(cipher_hex, xor_key):
    """Полный brute force без известных символов"""
    print("🎯 ПОЛНЫЙ BRUTE FORCE...")
    
    # Расширенный набор символов
    all_chars = (
        string.digits +
        string.ascii_letters + 
        string.punctuation
    )
    
    total_combinations = len(all_chars) ** 8
    print(f"📈 Всего комбинаций: {total_combinations:,}")
    
    # Расчет размера батча
    batch_size = calculate_batch_size(total_combinations)
    
    # Преобразуем зашифрованную строку
    cipher_bytes = bytes.fromhex(cipher_hex)
    target_numeric = 0
    for i, encrypted_char in enumerate(cipher_bytes):
        target_numeric |= (encrypted_char << (8 * (7 - i)))
    target_gpu = cp.uint64(target_numeric)
    
    start_time = time.time()
    ascii_codes = cp.array([ord(c) for c in all_chars], dtype=cp.uint64)
    
    try:
        found_password = None
        
        for batch_start in range(0, total_combinations, batch_size):
            if found_password:
                break
                
            batch_end = min(batch_start + batch_size, total_combinations)
            
            print(f"\n🔄 Батч {batch_start // batch_size + 1}: индексы {batch_start:,} - {batch_end:,}")
            
            # Создаем индексы для текущего батча
            indices_gpu = cp.arange(batch_start, batch_end, dtype=cp.uint64)
            
            # ГЕНЕРАЦИЯ ПАРОЛЕЙ НА GPU
            idx8 = indices_gpu % len(all_chars)
            idx7 = (indices_gpu // len(all_chars)) % len(all_chars)
            idx6 = (indices_gpu // (len(all_chars) * len(all_chars))) % len(all_chars)
            idx5 = (indices_gpu // (len(all_chars) ** 3)) % len(all_chars)
            idx4 = (indices_gpu // (len(all_chars) ** 4)) % len(all_chars)
            idx3 = (indices_gpu // (len(all_chars) ** 5)) % len(all_chars)
            idx2 = (indices_gpu // (len(all_chars) ** 6)) % len(all_chars)
            idx1 = (indices_gpu // (len(all_chars) ** 7)) % len(all_chars)
            
            # Получаем ASCII коды
            char1 = ascii_codes[idx1]
            char2 = ascii_codes[idx2] 
            char3 = ascii_codes[idx3]
            char4 = ascii_codes[idx4]
            char5 = ascii_codes[idx5]
            char6 = ascii_codes[idx6]
            char7 = ascii_codes[idx7]
            char8 = ascii_codes[idx8]
            
            # Собираем пароли
            passwords_gpu = ((char1 << 56) | (char2 << 48) | (char3 << 40) | (char4 << 32) |
                            (char5 << 24) | (char6 << 16) | (char7 << 8) | char8)
            
            # Применяем XOR и сравниваем
            xor_key_64bit = cp.uint64(xor_key * 0x0101010101010101)
            encrypted_passwords = passwords_gpu ^ xor_key_64bit
            matches = cp.where(encrypted_passwords == target_gpu)[0]
            
            if len(matches) > 0:
                found_idx_in_batch = int(matches[0])
                found_idx_global = batch_start + found_idx_in_batch
                
                # Восстанавливаем пароль
                found_val = int(passwords_gpu[found_idx_in_batch])
                found_chars = [
                    chr((found_val >> 56) & 0xFF),
                    chr((found_val >> 48) & 0xFF),
                    chr((found_val >> 40) & 0xFF),
                    chr((found_val >> 32) & 0xFF),
                    chr((found_val >> 24) & 0xFF),
                    chr((found_val >> 16) & 0xFF),
                    chr((found_val >> 8) & 0xFF),
                    chr(found_val & 0xFF)
                ]
                found_password = ''.join(found_chars)
                
                total_time = time.time() - start_time
                
                print(f"✅ ПАРОЛЬ НАЙДЕН: '{found_password}'")
                print(f"   Глобальный индекс: {found_idx_global:,}")
                print(f"   Общее время: {total_time:.3f}с")
                break
            else:
                elapsed = time.time() - start_time
                progress = (batch_end / total_combinations) * 100
                speed = batch_end / elapsed if elapsed > 0 else 0
                
                print(f"   Прогресс: {progress:.6f}%")
                print(f"   Скорость: {speed:,.0f} комб/сек")
        
        if not found_password:
            total_time = time.time() - start_time
            print(f"\n❌ Пароль не найден")
            
        return found_password
        
    except Exception as e:
        print(f"❌ Ошибка: {e}")
        import traceback
        traceback.print_exc()
        return None

def find_key_with_known_char(cipher_hex, known_char, char_position=0):
    """Находит ключ по известному символу и шифру"""
    cipher_bytes = bytes.fromhex(cipher_hex)  # ДОБАВЛЕНО
    found_key = cipher_bytes[char_position] ^ ord(known_char)
    return found_key

def decrypt_with_partial_password(cipher_hex, known_positions, known_chars):
    """Дешифровка когда известна часть пароля"""
    cipher_bytes = bytes.fromhex(cipher_hex)
    
    # Находим ключ по известному символу
    # Формула: cipher = plain ^ key => key = cipher ^ plain
    first_pos = min(known_positions.keys())
    correct_key = cipher_bytes[first_pos] ^ ord(known_chars[first_pos])
    
    print(f"🔍 Вычисление ключа: cipher[{first_pos}]({hex(cipher_bytes[first_pos])}) ^ '{known_chars[first_pos]}'({hex(ord(known_chars[first_pos]))}) = {hex(correct_key)}")
    
    # Дешифруем всё
    decrypted = bytes([b ^ correct_key for b in cipher_bytes])
    return decrypted.decode(), correct_key

def interactive_mode():
    """Интерактивный режим"""
    print("🚀 ИНТЕРАКТИВНЫЙ РЕЖИМ XOR BRUTE FORCE")
    print("=" * 60)
    
    # 1. Получаем исходные данные
    target_password = input("Введите искомое слово (8 символов): ").strip()
    if len(target_password) != 8:
        print("❌ Слово должно быть длиной 8 символов!")
        return
    
    key_input = input("Введите XOR ключ (в формате 0x13 или просто 19): ").strip()
    if key_input.startswith('0x'):
        xor_key = int(key_input, 16)
    else:
        xor_key = int(key_input)
    
    # 2. Шифруем слово
    cipher = xor_encrypt(target_password, xor_key)
    cipher_hex = cipher.hex()
    
    print(f"\n🔐 РЕЗУЛЬТАТ ШИФРОВАНИЯ:")
    print(f"   Исходное слово: {target_password}")
    print(f"   XOR ключ: {hex(xor_key)}")
    print(f"   Зашифрованная строка (hex): {cipher_hex}")
    
    # 3. Спрашиваем что известно для взлома
    print(f"\n🔍 ЧТО ИЗВЕСТНО ДЛЯ ВЗЛОМА?")
    print("1. Известен ключ и полный шифр")
    print("2. Известен только шифр (неизвестен ключ)")
    print("3. Известен шифр и часть пароля")
    
    choice = input("Выберите вариант (1-3): ").strip()
    
    result = None
    found_key = None
    
    if choice == "1":
        print(f"\n🎯 ВАРИАНТ 1: Известен ключ {hex(xor_key)} и шифр {cipher_hex}")
        print("🔓 МГНОВЕННАЯ ДЕШИФРОВКА...")
        result = decrypt_with_known_key(cipher_hex, xor_key)
        found_key = xor_key
        print(f"✅ Расшифрованный текст: {result}")
        
    elif choice == "2":
        print(f"\n🎯 ВАРИАНТ 2: Известен только шифр {cipher_hex} (ключ неизвестен)")
        print("🔑 ПОДБОР КЛЮЧА ЧЕРЕЗ ИЗВЕСТНЫЕ СИМВОЛЫ...")
        
        cipher_bytes = bytes.fromhex(cipher_hex)  # ДОБАВЛЕНО
        possible_first_chars = string.ascii_letters + string.digits
        found_solutions = []
        
        for first_char in possible_first_chars:
            try_key = cipher_bytes[0] ^ ord(first_char)
            decrypted = decrypt_with_known_key(cipher_hex, try_key)
            # Проверяем, что все символы разумные
            if all(c in (string.ascii_letters + string.digits + string.punctuation + ' ') for c in decrypted):
                found_solutions.append((decrypted, try_key))
                print(f"   Возможный ключ {hex(try_key)}: '{decrypted}'")
        
        if found_solutions:
            print(f"\n🎯 Найдено {len(found_solutions)} возможных решений")
            if len(found_solutions) == 1:
                result, found_key = found_solutions[0]
                print(f"✅ Вероятный пароль: {result}")
            else:
                print("⚠️  Несколько возможных решений, нужна дополнительная информация")
        else:
            print("❌ Не удалось найти подходящий ключ")
        
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
        
        key_known = input("Известен ли ключ? (y/n): ").strip().lower()
        
        if key_known == 'y':
            print("🔓 МГНОВЕННАЯ ДЕШИФРОВКА С ИЗВЕСТНЫМ КЛЮЧОМ...")
            result = decrypt_with_known_key(cipher_hex, xor_key)
            found_key = xor_key
        else:
            print("🔑 ВЫЧИСЛЕНИЕ КЛЮЧА ПО ИЗВЕСТНЫМ СИМВОЛАМ...")
            result, found_key = decrypt_with_partial_password(cipher_hex, known_positions, known_chars)
            print(f"✅ Найден ключ: {hex(found_key)}")
        
        print(f"✅ Расшифрованный текст: {result}")
    
    else:
        print("❌ Неверный выбор")
        return
    
    # Проверка результата
    if result and result == target_password:
        print(f"\n🎉 УСПЕХ! Пароль корректно взломан: {result}")
        if found_key:
            print(f"🔑 Использованный ключ: {hex(found_key)}")
    elif result:
        print(f"\n⚠️  Найден пароль: {result}, но он не соответствует исходному: {target_password}")
    else:
        print(f"\n💥 Пароль не найден")

# ЗАПУСК
if __name__ == "__main__":
    interactive_mode()
