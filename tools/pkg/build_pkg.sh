
echo "Building orange's libraries"

for dir in ./dep/*; do
    cd "$dir" || continue
    if [ -f "pkg.sh" ]; then
        echo Building $(cat info.txt)
        bash pkg.sh $1
    fi
    cd ../..
done

echo "Building orange's packages"

for dir in */; do
    cd "$dir" || continue
    if [ -f "pkg.sh" ]; then
        echo Building $(cat info.txt)
        bash pkg.sh $1
    fi
    cd ..
done

