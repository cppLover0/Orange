
echo "Building orange's packages"

max_depth=0
while IFS= read -r -d '' dir; do
    dir="${dir#./}"
    if [[ -z "$dir" ]]; then
        depth=0
    else
        depth=$(( $(grep -o "/" <<< "$dir" | wc -l) + 1 ))
    fi
    (( depth > max_depth )) && max_depth=$depth
done < <(find . -type d -print0)

if [[ $max_depth -lt 0 ]]; then
    max_depth=0
fi

for (( level=0; level<=max_depth; level++ )); do
    mapfile -d '' dirs < <(find . -mindepth $level -maxdepth $level -type d -print0 | sort -z)
    for dir in "${dirs[@]}"; do
        display_dir="${dir#./}"
        if [[ -f "$dir/pkg.sh" ]]; then
            if [[ -f "$dir/info.txt" ]]; then
                info=$(<"$dir/info.txt")
            else
                info="(no info.txt)"
            fi
            echo "Building $info in directory: $display_dir"
            (cd "$dir" && bash pkg.sh "$1")
        fi
    done
done
