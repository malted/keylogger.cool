# ./scripts/new-migration.sh <name>

if [ ! -f Cargo.toml ]
then
	echo "Aborted. You're in the wrong directory (no Cargo.toml found)"
	exit 1
fi

name="$*"
if [ -z "$name" ]
then
	echo "No migration name supplied"
	exit 1
fi

dir="migrations/$(date +%s)-$name"
mkdir -p "$dir"
touch "$dir/up.sql"
touch "$dir/down.sql"
echo "Created migration at $dir"
