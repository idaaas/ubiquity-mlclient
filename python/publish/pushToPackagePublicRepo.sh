#!/bin/bash

####################################################################################
##       Publish go package on github so it can be accessed by "go get" cmd       ##
####################################################################################

CREATE_TARGET_BRANCH_IF_NEEDED=true
DESTINATION_REPOSITORY_USERNAME=idaaas
DESTINATION_REPOSITORY_NAME=ubiquity-python-mlclient
GITHUB_SERVER=github.com
API_TOKEN_GITHUB=$1
SOURCE_DIRECTORY=$2
GIT_TAG=$3
TARGET_BRANCH=$4
COMMIT_MESSAGE="deployement of version $GIT_TAG of go wrapper"

GIT_CMD_REPOSITORY="https://$DESTINATION_REPOSITORY_USERNAME:$API_TOKEN_GITHUB@$GITHUB_SERVER/$DESTINATION_REPOSITORY_USERNAME/$DESTINATION_REPOSITORY_NAME.git"

git --version

CLONE_DIR=$(mktemp -d)

# Setup git
git config --global user.email "deploy@idaaas.com"
git config --global user.name "deploy"

# workaround for https://github.com/cpina/github-action-push-to-another-repository/issues/103
git config --global http.version HTTP/1.1

{
	git clone --single-branch --depth 1 --branch "$TARGET_BRANCH" "$GIT_CMD_REPOSITORY" "$CLONE_DIR"
} || {
    if [ "$CREATE_TARGET_BRANCH_IF_NEEDED" = "true" ]
    then
        # Default branch of the repository is cloned. Later on the required branch will be created
        git clone --single-branch --depth 1 "$GIT_CMD_REPOSITORY" "$CLONE_DIR"
    else
        false
    fi
} || {
	echo "::error::Could not clone the destination repository. Command:"
	echo "::error::git clone --single-branch --branch $TARGET_BRANCH $GIT_CMD_REPOSITORY $CLONE_DIR"
	echo "::error::(Note that if they exist USER_NAME and API_TOKEN is redacted by GitHub)"
	echo "::error::Please verify that the target repository exist AND that it contains the destination branch name, and is accesible by the API_TOKEN_GITHUB OR SSH_DEPLOY_KEY"
	exit 1
}

ls -la "$CLONE_DIR"

TEMP_DIR=$(mktemp -d)
# This mv has been the easier way to be able to remove files that were there
# but not anymore. Otherwise we had to remove the files from "$CLONE_DIR",
# including "." and with the exception of ".git/"
mv "$CLONE_DIR/.git" "$TEMP_DIR/.git"

# $TARGET_DIRECTORY is '' by default
ABSOLUTE_TARGET_DIRECTORY="$CLONE_DIR/$TARGET_DIRECTORY/"

echo "[+] Deleting $ABSOLUTE_TARGET_DIRECTORY"
rm -rf "$ABSOLUTE_TARGET_DIRECTORY"

echo "[+] Creating (now empty) $ABSOLUTE_TARGET_DIRECTORY"
mkdir -p "$ABSOLUTE_TARGET_DIRECTORY"

echo "[+] Listing Current Directory Location"
ls -al

echo "[+] Listing root Location"
ls -al /

mv "$TEMP_DIR/.git" "$CLONE_DIR/.git"

echo "[+] List contents of $SOURCE_DIRECTORY"
ls "$SOURCE_DIRECTORY"

echo "[+] Checking if local $SOURCE_DIRECTORY exist"
if [ ! -d "$SOURCE_DIRECTORY" ]
then
	echo "ERROR: $SOURCE_DIRECTORY does not exist"
	echo "This directory needs to exist when push-to-another-repository is executed"
	echo
	echo "In the example it is created by ./build.sh: https://github.com/cpina/push-to-another-repository-example/blob/main/.github/workflows/ci.yml#L19"
	echo
	echo "If you want to copy a directory that exist in the source repository"
	echo "to the target repository: you need to clone the source repository"
	echo "in a previous step in the same build section. For example using"
	echo "actions/checkout@v2. See: https://github.com/cpina/push-to-another-repository-example/blob/main/.github/workflows/ci.yml#L16"
	exit 1
fi

echo "[+] Copying contents of source repository folder $SOURCE_DIRECTORY to folder $TARGET_DIRECTORY in git repo $DESTINATION_REPOSITORY_NAME"
cp -ra "$SOURCE_DIRECTORY"/. "$CLONE_DIR/$TARGET_DIRECTORY"
cd "$CLONE_DIR"

echo "[+] Files that will be pushed"
ls -la

ORIGIN_COMMIT="https://$GITHUB_SERVER/$GITHUB_REPOSITORY/commit/$GITHUB_SHA"
COMMIT_MESSAGE="${COMMIT_MESSAGE/ORIGIN_COMMIT/$ORIGIN_COMMIT}"
COMMIT_MESSAGE="${COMMIT_MESSAGE/\$GITHUB_REF/$GITHUB_REF}"

echo "[+] Set directory is safe ($CLONE_DIR)"
# Related to https://github.com/cpina/github-action-push-to-another-repository/issues/64
git config --global --add safe.directory "$CLONE_DIR"

if [ "$CREATE_TARGET_BRANCH_IF_NEEDED" = "true" ]
then
    echo "[+] Switch to the TARGET_BRANCH"
    # || true: if the $TARGET_BRANCH already existed in the destination repo:
    # it is already the current branch and it cannot be switched to
    # (it's not needed)
    # If the branch did not exist: it switches (creating) the branch
	# For ubuntu 16.04 we needed to create a new branch manually
    git switch -c "$TARGET_BRANCH" || true
fi

echo "[+] Adding git commit"
git add .

echo "[+] git status:"
git status

echo "[+] git diff-index:"
# git diff-index : to avoid doing the git commit failing if there are no changes to be commit
git diff-index --quiet HEAD || git commit --message "$COMMIT_MESSAGE"

echo "[+] git tag with $GIT_TAG"
git tag $GIT_TAG

echo "[+] Pushing git commit"
# --set-upstream: sets de branch when pushing to a branch that does not exist
git push -f "$GIT_CMD_REPOSITORY" --set-upstream "$TARGET_BRANCH" --tags