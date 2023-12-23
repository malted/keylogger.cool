
if gh auth status | grep -q "✓ Logged in to github.com as "; then
	VERSION=$(gh api https://api.github.com/repos/malted/keylogger.cool/releases --jq '.[0].name')
	URL=$(gh api https://api.github.com/repos/malted/keylogger.cool/releases --jq '.[0].assets[0].browser_download_url')
else
	echo "Please login to GitHub"
	exit 1
fi

# GH=$(curl -s https://api.github.com/repos/malted/keylogger.cool/releases)
# VERSION=$(echo $GH | grep -o '"name": "[^"]*' | head -1 | sed 's/"name": "//')
# URL=$(echo $GH | grep -m 1 '"browser_download_url":' | sed -E 's/.*"browser_download_url": "([^"]+)".*/\1/')

kc_install="$HOME/.klcool"
bin_dir="$kc_install/bin"
exe="$bin_dir/klcool"

if [ ! -d "$bin_dir" ]; then
	mkdir -p "$bin_dir"
fi

LGREY='\033[1;37m'
ORANGE='\033[0;33m'
GREEN='\033[0;32m'
LGREEN='\033[1;32m'
NC='\033[0m' # No Colour
function typewrite {
	printf "$1"
	text="$2"
	for (( i=0; i<${#text}; i++ )); do
		printf "${text:$i:1}"
		sleep 0.01
	done
	printf "\n"
}

typewrite "\n${GREEN}Downloading${NC} " "keylogger.cool ${VERSION}"
printf "${LGREY}"
curl -fSL --progress-bar $URL -o $exe
printf "\033[1A\033[2K"
printf "\033[1A\033[2K"
typewrite "${LGREEN}Downloaded${NC}  " "keylogger.cool ${VERSION}"
typewrite "${LGREEN}Installed${NC}   " "to ${exe/#$HOME/~}"

chmod +x $exe
xattr -dr com.apple.quarantine $exe # Code signing is cringe
typewrite "${LGREEN}Disabled${NC}    " "execution quarantine"

IDENT="cool.keylogger.app"
PLIST_PATH="$HOME/Library/LaunchAgents/${IDENT}.plist"
cat << EOF > $PLIST_PATH
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>${IDENT}</string>

    <key>Program</key>
    <string>${exe}</string>

    <key>KeepAlive</key>
    <true/>

    <key>ThrottleInterval</key>
    <integer>5</integer>

    <key>RunAtLoad</key>
    <true/>

    <key>StandardOutPath</key>
    <string>/tmp/klcool.out</string>
    <key>StandardErrorPath</key>
    <string>/tmp/klcool.err</string>
</dict>
</plist>
EOF

if [ $(launchctl list | grep $IDENT | wc -l) -gt 0 ]; then
	launchctl unload ${PLIST_PATH}
	launchctl load ${PLIST_PATH}
	typewrite "${LGREEN}Reloaded${NC}    " "launchd agent"
else
	launchctl load ${PLIST_PATH}
	typewrite "${LGREEN}Loaded${NC}      " "launchd agent"
fi
