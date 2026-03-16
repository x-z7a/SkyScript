#!/bin/sh

set -eu

# ask if it's major/minor/patch release (if no argument is given)
if [ $# -ne 0 ]; then
  release_type=$1
else
    echo "What type of release is this? (major/minor/patch)"
    read release_type
fi
# get the current version from git tags (remote)
current_version=$(git describe --tags --abbrev=0)
echo "Current version: $current_version"
# split the version into major, minor, patch
IFS='.' read -r major minor patch <<< "$current_version"    
# increment the version based on the release type
case $release_type in
  major)
    major=$((major + 1))
    minor=0
    patch=0
    ;;
  minor)
    minor=$((minor + 1))
    patch=0
    ;;
  patch)
    patch=$((patch + 1))
    ;;
  *)
    echo "Invalid release type. Use major, minor, or patch."
    exit 1
    ;;
esac
# create the new version string
new_version="$major.$minor.$patch"
echo "New version: $new_version"
# create a new git tag for the new version
git tag -a "$new_version" -m "Release version $new_version"
# push the new tag to the remote repository
git push
git push origin "$new_version"  
