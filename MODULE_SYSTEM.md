# Velocity Compiler V2 - With Module/Import System! 🚀

## 🎉 NEW FEATURE: Module System (Like C's #include or Rust's use)

Now you can **import libraries** and reuse code across files!

## 🔤 Import Keyword: `anaw` (اَناو)

**anaw** = "to bring" in Kashmiri - perfect for importing modules!

## 📚 How It Works

### Basic Import Syntax

```kashmiri
anaw math;        # Import the math library
anaw io;          # Import the I/O library
anaw mylib;       # Import your custom library

kar main() -> adad {
    ath result = math.shakti(2, 10);  # Use math.power
    chu result;
}
```

### Module Function Calls

Use dot notation to call functions from imported modules:

```kashmiri
module_name.function_name(args)
```

## 🗂️ Module Search Paths

Velocity searches for modules in this order:

1. **Current directory** - where your main file is
2. **Standard library** - `/usr/local/lib/velocity/` (or custom path)
3. **Custom paths** - set via `VELOCITY_STDLIB` environment variable

### Setting Custom Library Path

```bash
export VELOCITY_STDLIB=/path/to/your/libraries
./velocity myprogram.vel
```

## 📖 Standard Library Modules

### 1. **math.vel** - Mathematical Functions

```kashmiri
anaw math;

kar main() -> adad {
    # Absolute value (mutlaq = مُطلَق)
    ath x = math.mutlaq(-42);  # Returns 42
    
    # Maximum (azeem = اَعظَم)
    ath max = math.azeem(10, 20);  # Returns 20
    
    # Minimum (asgar = اَصغَر)
    ath min = math.asgar(10, 20);  # Returns 10
    
    # Power (shakti = شَکتی)
    ath power = math.shakti(2, 10);  # Returns 1024
    
    # Square root (jar = جَر)
    ath sqrt = math.jar(16);  # Returns 4
    
    # Factorial (zarb_tartib = ضَرب تَرتیٖب)
    ath fact = math.zarb_tartib(5);  # Returns 120
    
    # GCD (mushtarak_qasim = مُشتَرَک قاسِم)
    ath gcd = math.mushtarak_qasim(48, 18);  # Returns 6
    
    # Check if prime (awwal_chu = اَوَّل چھ)
    ath is_prime = math.awwal_chu(17);  # Returns 1 (true)
    
    # Fibonacci
    ath fib = math.fibonacci(10);  # Returns 55
    
    # Sum from 1 to n (jama_tak = جَمع تَک)
    ath sum = math.jama_tak(100);  # Returns 5050
    
    # Check if even (joft_chu = جَفت چھ)
    ath is_even = math.joft_chu(42);  # Returns 1 (true)
    
    # Check if odd (taaq_chu = طاق چھ)
    ath is_odd = math.taaq_chu(43);  # Returns 1 (true)
    
    chu power;
}
```

### 2. **io.vel** - Input/Output (Coming Soon)

```kashmiri
anaw io;

kar main() -> adad {
    ath x = 42;
    io.chhaap("%d\n", x);  # Print integer
    io.chhaapLine();       # Print newline
    chu 0;
}
```

## 🛠️ Creating Your Own Libraries

### Step 1: Create a Module File

**mymath.vel**:
```kashmiri
# My custom math functions

kar double(x: adad) -> adad {
    chu x * 2;
}

kar triple(x: adad) -> adad {
    chu x * 3;
}

kar square(x: adad) -> adad {
    chu x * x;
}
```

### Step 2: Import and Use It

**main.vel**:
```kashmiri
anaw mymath;

kar main() -> adad {
    ath x = 10;
    ath doubled = mymath.double(x);    # 20
    ath tripled = mymath.triple(x);    # 30
    ath squared = mymath.square(x);    # 100
    
    chu squared;
}
```

### Step 3: Compile

```bash
# Make sure both files are in the same directory
./velocity main.vel -o main
./main
echo $?  # Prints 100
```

## 📂 Organizing Your Libraries

### Recommended Structure

```
my_project/
├── main.vel           # Your main program
├── libs/              # Your custom libraries
│   ├── mymath.vel
│   ├── mystring.vel
│   └── myutils.vel
└── build/             # Compiled output
```

### Using Libraries in Subdirectories

```kashmiri
# Import from libs/ subdirectory
anaw libs/mymath;

kar main() -> adad {
    chu libs/mymath.square(5);
}
```

## 🌟 Multiple Imports

You can import as many modules as you need:

```kashmiri
anaw math;
anaw io;
anaw mylib;
anaw utils;

kar main() -> adad {
    ath x = math.shakti(2, 5);
    ath y = mylib.process(x);
    ath z = utils.validate(y);
    chu z;
}
```

## ⚡ Benefits of Module System

### 1. **Code Reusability**
Write once, use everywhere!

```kashmiri
# Instead of copying functions
kar abs(x: adad) -> adad { ... }

# Just import
anaw math;
ath result = math.mutlaq(x);
```

### 2. **Organization**
Keep your code clean and organized:

```
- math.vel      → All math functions
- string.vel    → String utilities
- file.vel      → File operations
- network.vel   → Network functions
```

### 3. **Collaboration**
Share libraries with others:

```bash
# Install a library from GitHub
git clone https://github.com/user/velocity-json
export VELOCITY_STDLIB=./velocity-json
```

### 4. **Namespace Management**
Avoid function name conflicts:

```kashmiri
anaw math;
anaw stats;

# No conflict even if both have 'average' function
ath x = math.average(data);
ath y = stats.average(data);
```

## 📦 Creating a Standard Library Package

### Directory Structure

```
/usr/local/lib/velocity/
├── math.vel
├── io.vel
├── string.vel
├── file.vel
├── array.vel
└── README.md
```

### Installation

```bash
# Install standard library
sudo mkdir -p /usr/local/lib/velocity
sudo cp stdlib/*.vel /usr/local/lib/velocity/

# Now available globally
./velocity myprogram.vel
```

## 🔍 How Module Resolution Works

1. **Check current directory**
   ```
   ./mylib.vel
   ```

2. **Check standard library**
   ```
   /usr/local/lib/velocity/mylib.vel
   ```

3. **Check custom paths**
   ```
   $VELOCITY_STDLIB/mylib.vel
   ```

4. **Error if not found**
   ```
   Error: Module not found: mylib
   ```

## 🎯 Real-World Example

**calculator.vel** (library):
```kashmiri
kar jama(a: adad, b: adad) -> adad {
    chu a + b;
}

kar tafreeq(a: adad, b: adad) -> adad {
    chu a - b;
}

kar zarab(a: adad, b: adad) -> adad {
    chu a * b;
}

kar taqseem(a: adad, b: adad) -> adad {
    agar b == 0 {
        chu 0;
    }
    chu a / b;
}
```

**app.vel** (main program):
```kashmiri
anaw calculator;
anaw math;

kar main() -> adad {
    ath a = 100;
    ath b = 25;
    
    ath sum = calculator.jama(a, b);       # 125
    ath diff = calculator.tafreeq(a, b);   # 75
    ath prod = calculator.zarab(a, b);     # 2500
    ath quot = calculator.taqseem(a, b);   # 4
    
    ath result = math.azeem(sum, prod);    # 2500
    
    chu result;
}
```

## 🚀 Advanced Features

### Circular Import Prevention

The compiler automatically prevents circular imports:

```kashmiri
# a.vel
anaw b;  # OK

# b.vel
anaw a;  # Error: Circular dependency detected
```

### Import Caching

Each module is only loaded once, even if imported multiple times:

```kashmiri
anaw math;
anaw math;  # Ignored - already loaded
anaw math;  # Ignored - already loaded
```

## 📝 Best Practices

### 1. **One Module = One Purpose**
```
✅ math.vel      - Mathematical functions only
❌ utils.vel     - Everything mixed together
```

### 2. **Clear Naming**
```kashmiri
✅ anaw string_utils;
✅ anaw network_http;
❌ anaw stuff;
❌ anaw misc;
```

### 3. **Document Your Modules**
```kashmiri
# Math Library
# Provides basic mathematical operations
# Author: Your Name
# Version: 1.0

kar square(x: adad) -> adad {
    chu x * x;
}
```

### 4. **Group Related Functions**
```kashmiri
# Good organization
anaw math/basic;      # +, -, *, /
anaw math/advanced;   # sqrt, pow, etc.
anaw math/stats;      # mean, median, etc.
```

## 🎓 Learning Path

1. **Start Simple**: Use built-in math library
2. **Create Your Own**: Make a simple utility library
3. **Share Code**: Organize functions into modules
4. **Build Packages**: Create reusable library collections

## 🔮 Future Enhancements

- [ ] Version management for libraries
- [ ] Package manager (like npm, cargo)
- [ ] Remote imports (download from URLs)
- [ ] Compiled library cache
- [ ] Module aliasing (`anaw math as m;`)

## 💪 Why This is Powerful

### Before (Without Imports):
```kashmiri
# Every file needs its own abs() function
kar abs(x: adad) -> adad {
    agar x < 0 { chu -x; }
    chu x;
}

kar main() -> adad {
    ath x = abs(-42);
    chu x;
}
```

### After (With Imports):
```kashmiri
anaw math;

kar main() -> adad {
    chu math.mutlaq(-42);
}
```

**Much cleaner! Much more powerful!**

---

## 🎉 Summary

The module system transforms Velocity from a basic language into a **powerful, extensible platform**. Now you can:

✅ Import standard libraries
✅ Create your own libraries  
✅ Share code across projects
✅ Build on others' work
✅ Organize large projects
✅ Avoid code duplication

**This is just like `#include` in C or `use` in Rust, but with Kashmiri keywords!**

اَسۍ کٲشُر زَبانَس چھُ تَکنالوجی مَنٛز جاے دِوان! 🚀
