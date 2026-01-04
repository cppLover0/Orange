
while true; do

    stty echo

    echo "Select program to start:"
    echo "1) i3wm"
    echo "2) twm"
    echo "3) xfce4"
    echo "4) bash"
    read -p "Select number: " choice

    stty -echo

    case "$choice" in
        1)
            xinit /etc/X11/i3rc > /dev/null 2> /dev/null
            ;;
        2)
            xinit /etc/X11/twmrc > /dev/null 2> /dev/null
            ;;
        3)
            xinit /etc/X11/xfcerc > /dev/null 2> /dev/null
            ;;
        4)
            stty echo
            bash
            ;;
        *)
            echo "Wrong choice !"
            echo ""
            ;;
    esac

    stty echo

done
