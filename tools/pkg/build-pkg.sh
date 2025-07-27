echo "Building orange's packages"

max_depth=0
for dir in $(find . -type d | sed 's|^\./||' | grep -v '^$'); do
    depth=$(echo "$dir" | awk -F'/' '{print NF}')
    if (( depth > max_depth )); then
        max_depth=$depth
    fi
done

for ((level=1; level<=max_depth; level++)); do
    for dir in $(find . -mindepth $level -maxdepth $level -type d | sed 's|^\./||'); do
        cd "$dir" || continue
        if [ -f "pkg.sh" ]; then
            echo Building $(cat info.txt)
            bash pkg.sh $1
        fi
        cd - > /dev/null
    done
done