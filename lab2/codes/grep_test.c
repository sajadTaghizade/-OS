#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  char buf[1024]; // یک بافر بزرگ برای نگهداری خط پیدا شده
  int len;

  printf(1, "--- grep_syscall Test ---\n");

  // ===============================================
  // تست ۱: حالت موفقیت (کلمه‌ای که وجود دارد)
  // ===============================================
  printf(1, "Test 1: Searching for 'xv6' in 'README' (should succeed)\n");
  
  // آرگومان‌ها: (keyword, filename, user_buffer, buffer_size)
  // [cite: 3280, 3312]
  len = grep_syscall("xv6", "README", buf, sizeof(buf)); 

  if (len > 0) {
    printf(1, "SUCCESS: Found line (len %d): %s\n", len, buf);
  } else {
    printf(1, "FAILED: Word 'xv6' not found (ret=%d)\n", len);
  }

  // ===============================================
  // تست ۲: حالت شکست (کلمه‌ای که وجود ندارد)
  // ===============================================
  printf(1, "\nTest 2: Searching for 'nonexistent_word' in 'README' (should fail)\n");
  
  len = grep_syscall("nonexistent_word", "README", buf, sizeof(buf));

  if (len == -1) {
    printf(1, "SUCCESS: Syscall correctly returned %d (word not found)\n", len);
  } else {
    printf(1, "FAILED: Syscall returned %d, but expected -1\n", len);
  }

  // ===============================================
  // تست ۳: حالت شکست (فایلی که وجود ندارد)
  // ===============================================
  printf(1, "\nTest 3: Searching in 'nonexistent_file' (should fail)\n");
  
  len = grep_syscall("xv6", "nonexistent_file", buf, sizeof(buf));

  if (len == -1) {
    printf(1, "SUCCESS: Syscall correctly returned %d (file not found)\n", len);
  } else {
    printf(1, "FAILED: Syscall returned %d, but expected -1\n", len);
  }

  printf(1, "--- grep_syscall Test Complete ---\n");
  exit();
}