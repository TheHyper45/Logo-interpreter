# Logo interpreter

This is an interpreter of a Logo-like programming language written in C++ that I'm making as a project to pass the first semester of programming class at the university I'm studying.

## Features

This program takes a script written in the custom Logo-like programming language and outputs a bitmap that contains the path travelled by the virtual turtle as defined in the script.

### Features supported by the programming language:
- Variables
- References
- User-defined and built-in functions
- Control flow primitives (if, while, for, break, continue and return statements)
- Recursive functions
- printf-like function for outputting stuff to the console
- Functions for manipulating the canvas (moving and rotating turtle, saving the canvas to a file etc.)

## Usage

```bash
./logo name_of_your_script.txt
```

## Compiling

To compile this you need CMake, make (Linux) or Visual Studio (Windows, any version that supports C++20).
```bash
cmake -S [source_path] -B [build_path] -CMAKE_BUILD_TYPE=[build type]
cd [build_path]
make
```

## License

This project is released under the MIT license.

## Example 1

### Input
<!--I chose 'nim' as a language in the listing because it has the same keywords as my language. It even uses the same syntax for comments.-->
```nim
let angle = radians(70);
let angle2 = radians(7);

#This draws a nice fern in a recursive fashion.
func fern(size,sign) {
	if size < 1 { return; }
	forward(size);
	#Operator ' is for looking up the variable in the scope above outside the function.
	#Functions don't automatically capture variables from the enclosing scope.
	right('angle * sign);
	fern(size * 0.5,sign * -1.0);
	left('angle * sign);
	forward(size);
	left('angle * sign);
	fern(size * 0.5,sign);
	right('angle * sign);
	right('angle2 * sign);
	fern(size - 1,sign);
	left('angle2 * sign);
	backwards(size * 2);
}

#init(width,height,background_r,background_g,background_b)
init(1024,1024,0,0,0);
pencolor(255,255,255);
left(radians(90));
backwards(128);
fern(25,1);
save("output.bmp");
```

### Output
![Image](./output.bmp)

## Example 2

### Input
```nim
let int_v = 10;
let float_v = 0.125;
let bool_v = true;
let string_v = "Hello from Logo!";
let ref_v = &float_v;

print("typename(int_v) == \"%\"\n",typename(int_v));
print("typename(float_v) == \"%\"\n",typename(float_v));
print("typename(bool_v) == \"%\"\n",typename(bool_v));
print("typename(string_v) == \"%\"\n",typename(string_v));
print("typename(ref_v) == \"%\"\n",typename(ref_v));
print("typename(^ref_v) == \"%\"\n",typename(^ref_v));
```

### Output
```bash
typename(int_v) == "Int"
typename(float_v) == "Float"
typename(bool_v) == "Bool"
typename(string_v) == "String"
typename(ref_v) == "Reference"
typename(^ref_v) == "Float"
```

## Example 3

### Input
```nim
#Loop bounds are left inclusive, right exclusive.
for i : 1 -> 51 {
	if i % 15 == 0 {
		print("fizzbuzz\n");
	}
	else if i % 3 == 0 {
		print("fizz\n");
	}
	else if i % 5 == 0 {
		print("buzz\n");
	}
	else print("%\n",i);
}
```

### Output
```bash
1
2
fizz
4
buzz
fizz
7
8
fizz
buzz
11
fizz
13
14
fizzbuzz
16
17
fizz
19
buzz
fizz
22
23
fizz
buzz
26
fizz
28
29
fizzbuzz
31
32
fizz
34
buzz
fizz
37
38
fizz
buzz
41
fizz
43
44
fizzbuzz
46
47
fizz
49
buzz
```

## TODO

This is a list of features that I wanted to implement but couldn't because of lack of time. I'm not sure if I'm going to implement these features in the future.

- Arrays (they parse correctly but the interpreter doesn't recognize them)
- Proper strings and string manipulation functions
- Structs
- File I/O
