
echo "Building orange's packages"

export PATH="$HOME/opt/cross/orange/bin:$PATH"

if [ ! "$(which x86_64-orange-mlibc-gcc)" ]; then
    echo "It looks like you don't have the cross-compiler installed, or it isn't in your PATH."
    echo "If you built your cross-compiler, add it to your PATH with:"
    echo 'export PATH="$HOME/opt/cross/orange/bin:$PATH"'
    echo 'Alternatively, you can build the cross-compiler with: sh build-cross.sh'
    echo 'Also you should have host gcc ~13 or ~ 14'
fi

for dir in {0..14}; do
	cd "$dir"
	for pkg_dir in */; do
		cd "$pkg_dir"
		echo "Building $(cat info.txt)"
		bash pkg.sh "$1"
		cd ..
	done
	cd ..
done

echo Done
