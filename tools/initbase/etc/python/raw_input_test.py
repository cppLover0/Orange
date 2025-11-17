import sys
import tty
import termios

def get_char():
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)  
        ch = sys.stdin.read(1)  
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)  
    return ch

print("Write something (q - quit):")
while True:
    char = get_char()
    print(f"You pressed: {repr(char)}")
    if char == 'q':
        print("Exiting")
        break