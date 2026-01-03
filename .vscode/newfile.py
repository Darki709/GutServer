import os
import sys

def create_class_files(folder, class_name):
    # Ensure folder exists
    os.makedirs(folder, exist_ok=True)

    # File paths
    hpp_file = os.path.join(folder, f"{class_name}.hpp")
    cpp_file = os.path.join(folder, f"{class_name}.cpp")

    # Create .hpp file with include guards
    include_guard = f"{class_name.upper()}_HPP"
    hpp_content = f"""#ifndef {include_guard}
#define {include_guard}

class {class_name} {{
public:
    {class_name}();
    ~{class_name}();

private:

}};

#endif // {include_guard}
"""
    with open(hpp_file, 'w') as f:
        f.write(hpp_content)

    # Create .cpp file including the header
    cpp_content = f"""#include "{class_name}.hpp"

{class_name}::{class_name}() {{
    // Constructor
}}
"""
    
    with open(cpp_file, 'w') as f:
        f.write(cpp_content)

    print(f"Created:\n  {hpp_file}\n  {cpp_file}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python create_class.py <folder_path> <ClassName>")
        sys.exit(1)

    folder_path = sys.argv[1]
    class_name = sys.argv[2]

    create_class_files(folder_path, class_name)