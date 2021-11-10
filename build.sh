out_dir=build

if ! [ -d $out_dir ]; then
	mkdir $out_dir
fi

g++ -g -Wall -o $out_dir/mas src/*.cpp
