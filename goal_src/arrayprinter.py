def generate_inline_array(count):
    header = f"(new 'static 'inline-array vector {count}"
    body = "\n" + "\n".join([f"  (new 'static 'vector)" for _ in range(count)])
    footer = "\n)"
    return header + body + footer

def main():
    try:
        count = int(input("How many vectors do you want? "))
        if count <= 0:
            print("Please enter a positive integer.")
            return
        result = generate_inline_array(count)
        print("\nGenerated OpenGOAL inline-array:\n")
        print(result)
    except ValueError:
        print("Invalid input. Please enter a number.")

if __name__ == "__main__":
    main()
