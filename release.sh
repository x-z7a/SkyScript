#!/bin/bash

set -eu

current_branch=$(git rev-parse --abbrev-ref HEAD)

# Get the latest stable version tag (vX.Y.Z only, no pre-release)
latest_stable=$(git tag --list 'v[0-9]*.[0-9]*.[0-9]*' --sort=-v:refname | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | head -1)
if [ -z "$latest_stable" ]; then
  echo "Error: no stable version tag found."
  exit 1
fi

echo "Current stable version: $latest_stable"

if [ "$current_branch" = "main" ]; then
  # --- Main branch: normal semver bump ---
  if [ $# -ne 0 ]; then
    release_type=$1
  else
    echo "What type of release is this? (major/minor/patch)"
    read -r release_type
  fi

  # Strip 'v' prefix and split
  version="${latest_stable#v}"
  IFS='.' read -r major minor patch <<< "$version"

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

  new_version="v$major.$minor.$patch"
else
  # --- Non-main branch: vX.Y.Z-BRANCH.N ---
  # Sanitize branch name for use in tag (replace non-alphanumeric with -)
  branch_slug=$(echo "$current_branch" | sed 's/[^a-zA-Z0-9]/-/g' | sed 's/--*/-/g' | sed 's/^-//;s/-$//')

  # Find the highest existing N for this base-branch combo
  prefix="${latest_stable}-${branch_slug}."
  last_n=$(git tag --list "${prefix}*" | sed "s|^${prefix}||" | grep -E '^[0-9]+$' | sort -n | tail -1)

  if [ -z "$last_n" ]; then
    next_n=0
  else
    next_n=$((last_n + 1))
  fi

  new_version="${latest_stable}-${branch_slug}.${next_n}"
fi

echo "New version: $new_version"
git tag -a "$new_version" -m "Release version $new_version"
git push
git push origin "$new_version"
