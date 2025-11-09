import cupy as cp
import string
import time
import numpy as np
import psutil

def calculate_batch_size(total_combinations, chars_count=62, password_length=8):
    """Рассчитывает оптимальный размер батча на основе доступной памяти GPU"""
    
    # Размер одного пароля в байтах (uint64 = 8 байт)
    password_size_bytes = 8
    
    # Доступная память GPU (берем 80% чтобы не перегружать)
    try:
        mem_info = cp.cuda.runtime.memGetInfo()
        gpu_free_memory = mem_info[0]  # свободная память в байтах
        gpu_total_memory = mem_info[1] # общая память в байтах
        
        # Используем 70% свободной памяти для безопасности
        available_memory = gpu_free_memory * 0.7
        
        print(f"🎯 Информация о памяти GPU:")
        print(f"   Общая память: {gpu_total_memory / (1024**3):.1f} GB")
        print(f"   Свободно: {gpu_free_memory / (1024**3):.1f} GB")
        print(f"   Доступно для батча: {available_memory / (1024**3):.1f} GB")
        
    except:
        # Если не можем получить информацию о GPU, используем консервативную оценку
        available_memory = 1 * 1024**3  # 1 GB по умолчанию
        print(f"⚠️  Не удалось получить информацию о GPU памяти, используем {available_memory / (1024**3):.1f} GB")
    
    # Максимальный размер батча в элементах
    max_batch_elements = int(available_memory // password_size_bytes)
    
    # Ограничим разумным размером (не более 100 миллионов за раз)
    max_batch_elements = min(max_batch_elements, 100_000_000)
    
    # Минимальный размер батча для эффективности GPU
    min_batch_elements = 1_000_000
    
    batch_size = max(min_batch_elements, max_batch_elements)
    
    print(f"📊 Расчет батча:")
    print(f"   Всего комбинаций: {total_combinations:,}")
    print(f"   Размер батча: {batch_size:,} паролей")
    print(f"   Память на батч: {(batch_size * password_size_bytes) / (1024**3):.2f} GB")
    print(f"   Количество батчей: {(total_combinations + batch_size - 1) // batch_size}")
    
    return batch_size

def real_gpu_brute_force_8char(self, target_password="12345678"):
    """GPU BRUTE FORCE для 8-значных паролей с батчами"""
    print("🎯 GPU BRUTE FORCE 8-ЗНАЧНЫХ ПАРОЛЕЙ...")
    
    # 1. Расширенный набор символов
    all_chars = (
        string.digits +           # 0-9 (10)
        string.ascii_letters +    # a-zA-Z (52)  
        string.punctuation        # !"#$%&'()*+,-./:;<=>?@[\]^_`{|}~ (32)
    )
    # Итого: 10 + 52 + 32 = 94 символа
    
    total_combinations = len(all_chars) ** 8
    num_chars = len(all_chars)
    
    print(f"🔤 Набор символов: {len(all_chars)} символов")
    print(f"   Цифры: 0-9 ({string.digits})")
    print(f"   Буквы: a-zA-Z ({string.ascii_letters})")
    print(f"   Спецсимволы: {string.punctuation}")
    print(f"📈 Всего комбинаций: {total_combinations:,}")
    print(f"🎯 Ищем: '{target_password}'")
    
    # 2. Расчет размера батча
    batch_size = calculate_batch_size(total_combinations, num_chars, 8)
    
    # 3. Подготовка целевого пароля
    target_numeric = 0
    for i, char in enumerate(target_password):
        target_numeric |= (ord(char) << (8 * (7 - i)))
    target_gpu = cp.uint64(target_numeric)
    
    print(f"🔢 Целевое значение: {target_numeric} (hex: {hex(target_numeric)})")
    
    start_time = time.time()
    ascii_codes = cp.array([ord(c) for c in all_chars], dtype=cp.uint64)
    
    try:
        # 4. ОБРАБОТКА БАТЧАМИ
        found_password = None
        
        for batch_start in range(0, total_combinations, batch_size):
            if found_password:
                break
                
            batch_end = min(batch_start + batch_size, total_combinations)
            current_batch_size = batch_end - batch_start
            
            print(f"\n🔄 Батч {batch_start // batch_size + 1}: индексы {batch_start:,} - {batch_end:,}")
            
            # Создаем индексы для текущего батча
            indices_gpu = cp.arange(batch_start, batch_end, dtype=cp.uint64)
            
            # 5. ГЕНЕРАЦИЯ ПАРОЛЕЙ НА GPU (8 позиций)
            idx8 = indices_gpu % num_chars
            idx7 = (indices_gpu // num_chars) % num_chars
            idx6 = (indices_gpu // (num_chars * num_chars)) % num_chars
            idx5 = (indices_gpu // (num_chars ** 3)) % num_chars
            idx4 = (indices_gpu // (num_chars ** 4)) % num_chars
            idx3 = (indices_gpu // (num_chars ** 5)) % num_chars
            idx2 = (indices_gpu // (num_chars ** 6)) % num_chars
            idx1 = (indices_gpu // (num_chars ** 7)) % num_chars
            
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
            
            # 6. ПОИСК НА GPU
            gpu_start = time.time()
            matches = cp.where(passwords_gpu == target_gpu)[0]
            gpu_time = time.time() - gpu_start
            
            # 7. ОБРАБОТКА РЕЗУЛЬТАТОВ
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
                progress = (batch_end / total_combinations) * 100
                
                print(f"✅ ПАРОЛЬ НАЙДЕН: '{found_password}'")
                print(f"   Глобальный индекс: {found_idx_global:,}")
                print(f"   Прогресс: {progress:.6f}%")
                print(f"   Общее время: {total_time:.3f}с")
                print(f"   Скорость: {batch_end/total_time:,.0f} комб/сек")
                break
            else:
                elapsed = time.time() - start_time
                progress = (batch_end / total_combinations) * 100
                speed = batch_end / elapsed if elapsed > 0 else 0
                
                print(f"   Прогресс: {progress:.6f}%")
                print(f"   Скорость: {speed:,.0f} комб/сек")
                print(f"   Осталось: ~{(total_combinations - batch_end) / speed / 3600:.1f} часов")
        
        if not found_password:
            total_time = time.time() - start_time
            print(f"\n❌ Пароль '{target_password}' не найден")
            print(f"   Проверено: {min(total_combinations, batch_end):,} комбинаций")
            print(f"   Общее время: {total_time:.3f}с")
            
        return found_password
        
    except Exception as e:
        print(f"❌ Ошибка: {e}")
        import traceback
        traceback.print_exc()
        return None

# Запуск
print("🚀 ЗАПУСК GPU BRUTE FORCE ДЛЯ 8-ЗНАЧНЫХ ПАРОЛЕЙ")
print("=" * 60)

class MockSelf:
    pass

# Можешь поменять target_password на любой 8-значный пароль
result = real_gpu_brute_force_8char(MockSelf(), target_password="12345678")

if result:
    print(f"\n🎉 УСПЕХ! Найден пароль: {result}")
else:
    print(f"\n💥 Пароль не найден")