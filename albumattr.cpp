/* albumattr - detects albums in folders and set some attributes
**
** Copyright (c) 2003-2004 pinc Software. All Rights Reserved.
*/


#include <Application.h>
#include <CheckBox.h>
#include <Alert.h>
#include <String.h>
#include <TranslationUtils.h>
#include <Bitmap.h>

#include <MediaFile.h>
#include <MediaTrack.h>

#include <Path.h>
#include <File.h>
#include <NodeInfo.h>
#include <Node.h>
#include <Directory.h>
#include <FindDirectory.h>

#include <kernel/fs_info.h>
#include <kernel/fs_attr.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "AlbumIcon.h"

static const char *kAlbumMimeString = "application/x-vnd.Be-directory-album";
static const char *kSettingsTitle = "Album Folder Settings";

static const uint32 kMsgAlbumFolderSettings = 'pAFA';
static const uint32 kMsgCreateCoverIconsChanged = 'cCic';

static const int32 kAudioFile = 1;
static const int32 kImageFile = 2;

class SettingsWindow : public BWindow {
	public:
		SettingsWindow(BRect rect);
		virtual ~SettingsWindow();

		virtual void MessageReceived(BMessage *message);

	private:
		BCheckBox *fUseMediaKit, *fCreateCoverIcons;
		BCheckBox *fAllowDifferentArtists, *fUseImageIcon;
		BCheckBox *fRecursive;
};

struct album_attrs {
	BString artist;
	BString album;
	BString genre;
	int32 length;
	int32 min_year;
	int32 max_year;
};

struct audio_attrs {
	BString artist;
	BString album;
	BString genre;
	int32 length;
	int32 year;
};


// these are the default settings - they may be superseded by the settings file
bool gRecursive = false;		// enter directories recursively
bool gVerbose = false;
bool gUseAlbumType = true;
bool gUseImageIcon = true;
bool gCreateCoverIcons = false;
bool gAllowDifferentArtists = false;
bool gForce = false;
bool gUseMediaKit = true;
bool gFromShell = false;
bool gHasSeenSettings = false;

BRect gSettingsWindowPosition(150, 150, 200, 200);


status_t
set_message_bool(BMessage &message, const char *name, bool value)
{
	if (message.ReplaceBool(name, value) != B_OK) {
		message.RemoveName(name);
		return message.AddBool(name, value);
	}

	return B_OK;
}


status_t
getSettingsPath(BPath &path)
{
	status_t status;
	if ((status = find_directory(B_USER_SETTINGS_DIRECTORY, &path)) != B_OK)
		return status;

	path.Append("pinc.albumattr settings");
	return B_OK;
}


status_t
saveSettings()
{
	status_t status;

	BPath path;
	if ((status = getSettingsPath(path)) != B_OK)
		return status;

	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if ((status = file.InitCheck()) != B_OK)
		return status;

	file.SetSize(0);

	BMessage save(kMsgAlbumFolderSettings);
	save.AddRect("settings position", gSettingsWindowPosition);

	save.AddBool("settings seen", true);
	save.AddBool("recursive", gRecursive);
	save.AddBool("different artists", gAllowDifferentArtists);
	save.AddBool("use media kit", gUseMediaKit);
	save.AddBool("create cover icons", gCreateCoverIcons);
	save.AddBool("use image icon", gUseImageIcon);

	return save.Flatten(&file);
}


void
readBool(BMessage &load, const char *name, bool &variable)
{
	bool value;
	if (load.FindBool(name, &value) == B_OK)
		variable = value;
}


status_t
readSettings()
{
	status_t status;

	BPath path;
	if ((status = getSettingsPath(path)) != B_OK)
		return status;

	BFile file(path.Path(), B_READ_ONLY);
	if ((status = file.InitCheck()) != B_OK) {
		fprintf(stderr, "albumattr: could not load settings: %s\n", strerror(status));
		return status;
	}

	BMessage load;
	if ((status = load.Unflatten(&file)) != B_OK)
		return status;

	if (load.what != kMsgAlbumFolderSettings)
		return B_ENTRY_NOT_FOUND;

	BRect rect;
	if (load.FindRect("settings position", &rect) == B_OK)
		gSettingsWindowPosition = rect;

	readBool(load, "settings seen", gHasSeenSettings);
	readBool(load, "recursive", gRecursive);
	readBool(load, "different artists", gAllowDifferentArtists);
	readBool(load, "use media kit", gUseMediaKit);
	readBool(load, "create cover icons", gCreateCoverIcons);
	readBool(load, "use image icon", gUseImageIcon);

	return B_OK;
}


//	#pragma mark -


status_t
writeAttributeString(BNode &node, const char *attribute, const char *value, bool overwrite)
{
	if (!overwrite) {
		attr_info attrInfo;

		status_t status = node.GetAttrInfo(attribute, &attrInfo);
		if (status == B_OK)
			return B_OK;
	}

	ssize_t size = node.WriteAttr(attribute, B_STRING_TYPE, 0, value, strlen(value) + 1);
	return size >= 0 ? B_OK : size;
}


status_t
readAttributeString(BNode &node, const char *attribute, BString &string)
{
	char *buffer = string.LockBuffer(1024);
	ssize_t size = node.ReadAttr(attribute, B_STRING_TYPE, 0, buffer, 1024);
	buffer[1023] = '\0';
	string.UnlockBuffer();

	if (size < B_OK)
		return size;

	return B_OK;
}


status_t
retrieveFromFile(BFile &file, audio_attrs &audioAttrs)
{
	readAttributeString(file, "Audio:Artist", audioAttrs.artist);
	readAttributeString(file, "Audio:Album", audioAttrs.album);
	readAttributeString(file, "Media:Genre", audioAttrs.genre);

	audioAttrs.length = 0;
	audioAttrs.year = 0;

	if (file.ReadAttr("Media:Year", B_INT32_TYPE, 0, &audioAttrs.year, sizeof(int32)) == sizeof(int32)
		&& audioAttrs.year != 0 && audioAttrs.year < 100)
		audioAttrs.year += 1900;

	BString lengthString;
	char *seconds;
	if (readAttributeString(file, "Media:Length", lengthString) == B_OK
		&& lengthString.String() != NULL
		&& (seconds = strchr(lengthString.String(), ':')) != NULL) {
		audioAttrs.length = atol(lengthString.String()) * 60 + atol(seconds + 1);
	} else {
		if (gVerbose)
			fprintf(stderr, "could not read Media:Length from file (%s)\n", lengthString.String());

		// retrieve length using the media kit (if we are allowed to)

		if (!gUseMediaKit)
			return B_OK;

		file.Seek(0, SEEK_SET);
		BMediaFile mediaFile(&file);

		if (mediaFile.InitCheck() == B_OK) {
			int32 numTracks = mediaFile.CountTracks();

			for (int32 i = 0; i < numTracks; i++) {
				BMediaTrack *track = mediaFile.TrackAt(i);
				if (track == NULL)
					continue;

				media_format format;
				if (track->EncodedFormat(&format) == B_OK
					&& format.type == B_MEDIA_ENCODED_AUDIO)
				{
					//audioAttrs->bitrate = (int32)(format.u.encoded_audio.bit_rate / 1000);
					audioAttrs.length = track->Duration() / 1000000;
					//audioAttrs->framerate = format.u.encoded_audio.output.frame_rate;
				}
				mediaFile.ReleaseTrack(track);
			}
		}
	}

	return B_OK;
}


int32
getFileType(BEntry &entry)
{
	char buffer[B_ATTR_NAME_LENGTH];
	BNode node(&entry);
	BNodeInfo info(&node);

	if (info.GetType(buffer) < B_OK) {
		// try to determine file type

		BPath path(&entry);
		update_mime_info(path.Path(), false, true, false);

		if (info.GetType(buffer) < B_OK)
			return -1;
	}

	if (!strncmp(buffer, "audio/", 6))
		return kAudioFile;

	if (!strncmp(buffer, "image/", 6))
		return kImageFile;

	return -1;
}


status_t
handleFile(BEntry &entry, audio_attrs &audioAttrs, int32 &fileType)
{
	char name[B_FILE_NAME_LENGTH];
	entry.GetName(name);

	// if it is not an audio file, return

	fileType = getFileType(entry);
	if (fileType != kAudioFile) {
		if (gVerbose)
			fprintf(stderr, "'%s' is not an audio file\n", name);
		return B_OK;
	}

	// Open File (on error, return showing error)

	BFile file(&entry, B_READ_ONLY);
	if (file.InitCheck() < B_OK) {
		fprintf(stderr, "could not open '%s'.\n", name);
		return B_IO_ERROR;
	}

	// retrieve attributes

	return retrieveFromFile(file, audioAttrs);
}


void
createIcon(BNodeInfo &targetInfo, BNodeInfo &imageInfo, BBitmap *source, icon_size type)
{
	BBitmap icon(BRect(0, 0, type - 1, type - 1), B_COLOR_8_BIT, true);
	if (targetInfo.GetIcon(&icon, type) == B_OK && !gForce)
		return;

	if (!gUseImageIcon || imageInfo.GetIcon(&icon, type) != B_OK) {
		BView *view = new BView(icon.Bounds(), "icon", B_FOLLOW_NONE, 0);

		icon.AddChild(view);
		icon.Lock();

		view->DrawBitmap(source, icon.Bounds());
		view->Sync();

		icon.Unlock();
	}

	targetInfo.SetIcon(&icon, type);
}


void
createCoverIcons(BEntry &target, entry_ref &image)
{
	BNode targetNode(&target);
	BNodeInfo targetInfo(&targetNode);
	if (targetInfo.InitCheck() != B_OK)
		return;

	BNode imageNode(&image);
	BNodeInfo imageInfo(&imageNode);
	if (imageInfo.InitCheck() != B_OK)
		return;

	BBitmap *cover = BTranslationUtils::GetBitmap(&image);
	if (cover != NULL) {
		createIcon(targetInfo, imageInfo, cover, B_MINI_ICON);
		createIcon(targetInfo, imageInfo, cover, B_LARGE_ICON);
	}
}


int32
countWordOccurences(const char *string, const char *word)
{
	size_t wordLength = strlen(word);
	int32 count = 0;

	while (string[0]) {
		if (!strncasecmp(string, word, wordLength)) {
			count++;
			string += wordLength;
		} else
			string++;
	}

	return count;
}


status_t
chooseCover(BMessage &refs, entry_ref &chosen)
{
	int32 count;
	refs.GetInfo("refs", NULL, &count);

	// are there any candidates at all?
	if (count == 0)
		return B_ENTRY_NOT_FOUND;

	// if there is only one image, we have a clear candidate
	if (count == 1)
		return refs.FindRef("refs", &chosen);

	const char *words[] = {"cover", "front", "album"};
	int32 wordCount = sizeof(words) / sizeof(const char *);

	const char *stopWords[] = {"back", "cd", "inlay", "inside", "logo", "single", "alternative"};
	int32 stopWordCount = sizeof(stopWords) / sizeof(const char *);

	int32 score[count];
	memset(score, 0, sizeof(score));

	// compute the score

	entry_ref ref;
	for (int32 i = 0; refs.FindRef("refs", i, &ref) == B_OK; i++) {
		BPath path(&ref);
		if (path.InitCheck() != B_OK)
			continue;

		for (int32 j = 0; j < wordCount; j++)
			score[i] += countWordOccurences(path.Path(), words[j]) * 2;

		// negative words have higher impact
		for (int32 j = 0; j < stopWordCount; j++)
			score[i] -= countWordOccurences(path.Path(), stopWords[j]) * 3;
	}

	// find the entry with the highest score

	int32 bestIndex = 0;
	int32 bestCount = 1;
	int32 bestScore = score[0];

	for (int32 i = 1; i < count; i++) {
		if (bestScore < score[i]) {
			bestIndex = i;
			bestScore = score[i];
			bestCount = 1;
		} else if (bestScore == score[i])
			bestCount++;
	}

	if (bestCount > 1) {
		// damn, we couldn't decide
		return B_ERROR;
	}

	return refs.FindRef("refs", bestIndex, &chosen);
}


int32
collectImages(BEntry &entry, BMessage &images)
{
	BDirectory directory(&entry);
	entry_ref ref;

	int32 count = 0;

	directory.Rewind();
	while (directory.GetNextRef(&ref) == B_OK) {
		BEntry sub(&ref, false);
		if (sub.IsDirectory()) {
			count += collectImages(sub, images);
		} else if (getFileType(sub) == kImageFile) {
			images.AddRef("refs", &ref);
			count++;
		}
	}

	return count;
}


bool
handleDirectory(BEntry &entry, int32 level)
{
	BPath path(&entry);

	if (!entry.IsDirectory()) {
		fprintf(stderr, "\"%s\" is not a directory\n", path.Path());
		return false;
	}

	BDirectory directory(&entry);
	BEntry entryIterator;

	album_attrs albumAttrs;
	albumAttrs.length = 0;
	albumAttrs.min_year = 0;
	albumAttrs.max_year = 0;

	BMessage images;

	int32 numAudioFiles = 0;
	bool differentArtists = false;
	bool differentAlbums = false;

	directory.Rewind();
	while (directory.GetNextEntry(&entryIterator, false) == B_OK) {
		audio_attrs audioAttrs;

		if (entryIterator.IsDirectory()) {
			bool wasAlbum = false;

			if (gRecursive)
				wasAlbum = handleDirectory(entryIterator, level + 1);

			if (wasAlbum && !gRecursive) {
				// if the sub-directory was an album, this won't be one
				return true;
			}

			continue;
		}

		int32 fileType;
		if (handleFile(entryIterator, audioAttrs, fileType) < B_OK)
			continue;

		if (fileType == kAudioFile) {
			if (numAudioFiles++ == 0) {
				// initialize album attributes
				albumAttrs.artist = audioAttrs.artist;
				albumAttrs.album = audioAttrs.album;
				albumAttrs.min_year = albumAttrs.max_year = audioAttrs.year;
				albumAttrs.genre = audioAttrs.genre;
			} else if (albumAttrs.artist != audioAttrs.artist)
				differentArtists = true;
			else if (albumAttrs.album != audioAttrs.album)
				differentAlbums = true;

			if (!audioAttrs.genre.ICompare("Soundtrack"))
				albumAttrs.genre = "Soundtrack";
			else if (audioAttrs.genre != albumAttrs.genre)
				albumAttrs.genre = "Misc";

			if (audioAttrs.length > 0)
				albumAttrs.length += audioAttrs.length;

			if (audioAttrs.year != 0) {
				if (audioAttrs.year > albumAttrs.max_year)
					albumAttrs.max_year = audioAttrs.year;
				else if (audioAttrs.year < albumAttrs.min_year)
					albumAttrs.min_year = audioAttrs.year;
			}
		}
	}

	if (numAudioFiles < 3) {
		if (gVerbose)
			fprintf(stderr, "Directory at \"%s\" is likely not to be an album - contains less than 3 files.\n", path.Path());

		return true;
			// this is no album, but it contains music files
	}

	if (!gAllowDifferentArtists && (differentArtists || differentAlbums)) {
		if (!gFromShell) {
			char message[1024];
			snprintf(message, sizeof(message),
				"The folder \"%s\" probably doesn't contain a music album. "
				"The %s attributes differ from file to file.\n\n"
				"If this folder contains a sound track to a movie or a sampler, "
				"this may be expected, but you might want to recheck the files",
				path.Path(),
					differentArtists && differentAlbums ? "artist and album" :
					differentArtists ? "artist" : "album");

			if (albumAttrs.genre.ICompare("Soundtrack")
				&& (new BAlert("Album Attributes", message,
						"Continue", "Cancel"))->Go() != 0) {
				return false;
			}
		} else {
			fprintf(stderr,
				"Directory at \"%s\" is not an album - Artist/Album differs from file to file.\n"
				"Use the -d option to allow setting the album attributes\n", path.Path());
			return false;
		}

		if (differentArtists)
			albumAttrs.artist = "Various";
	}

	if (gVerbose) {
		printf("Artist = \"%s\", Album = \"%s\", genre = %s, length = %02ld:%02ld, year = %ld - %ld\n",
			albumAttrs.artist.String(),
			albumAttrs.album.String(),
			albumAttrs.genre.String(),
			albumAttrs.length / 60,
			albumAttrs.length % 60,
			albumAttrs.min_year,
			albumAttrs.max_year);
	}

	// write back album information

	BNode node(&entry);

	if (gUseAlbumType) {
		// write new mime type
		node.WriteAttr("BEOS:TYPE", B_MIME_STRING_TYPE, 0, kAlbumMimeString, strlen(kAlbumMimeString) + 1);
	}

	writeAttributeString(node, "Album:Artist", albumAttrs.artist.String(), gForce);
	writeAttributeString(node, "Album:Title", albumAttrs.album.String(), gForce);
	writeAttributeString(node, "Album:Genre", albumAttrs.genre.String(), gForce);

	char buffer[64];
	sprintf(buffer, "%02ld:%02ld", albumAttrs.length / 60, albumAttrs.length % 60);
	writeAttributeString(node, "Album:Length", buffer, gForce);

	if (albumAttrs.min_year != 0 && albumAttrs.max_year != 0) {
		if (albumAttrs.min_year == albumAttrs.max_year)
			sprintf(buffer, "%4ld", albumAttrs.min_year);
		else
			sprintf(buffer, "%4ld-%4ld", albumAttrs.min_year, albumAttrs.max_year);

		writeAttributeString(node, "Album:Year", buffer, gForce);
	}

	if (gCreateCoverIcons && collectImages(entry, images) > 0) {
		entry_ref cover;
		if (chooseCover(images, cover) == B_OK)
			createCoverIcons(entry, cover);
	}

	return true;
}


//	#pragma mark -


void
installIcon(BMimeType &mime, const uint8 *bits, icon_size size)
{
	BBitmap bitmap(BRect(0, 0, size - 1, size - 1), B_COLOR_8_BIT);
	if (bitmap.InitCheck() != B_OK)
		return;

	memcpy(bitmap.Bits(), bits, size * size);

	mime.SetIcon(&bitmap, size);
}


void
addAttribute(BMessage &msg, char *name, char *publicName, int32 width, int32 type)
{
	msg.AddString("attr:name", name);
	msg.AddString("attr:public_name", publicName);
	msg.AddInt32("attr:type", type);
	msg.AddBool("attr:viewable", true);
	msg.AddBool("attr:editable", true);
	msg.AddInt32("attr:width", width);
	msg.AddInt32("attr:alignment", B_ALIGN_LEFT);
}


void
registerFileType()
{
	// First Set up the system mime type
	BMimeType mime(kAlbumMimeString);

	if (mime.InitCheck() != B_OK) {
		fputs("could not init mime type.\n", stderr);
		return;
	}
	bool installed = false;

	if (!mime.IsInstalled()) {
		mime.Install();
		installed = true;
	}

	// Add attributes to MIME type
	{
		BMessage msg;

		addAttribute(msg, "Album:Artist", "Artist", 120, B_STRING_TYPE);
		addAttribute(msg, "Album:Title", "Album", 180, B_STRING_TYPE);
		addAttribute(msg, "Album:Length", "Length", 50, B_STRING_TYPE);
		addAttribute(msg, "Album:Year", "Year", 60, B_STRING_TYPE);
		addAttribute(msg, "Album:Comment", "Comment", 80, B_STRING_TYPE);
		addAttribute(msg, "Album:Genre", "Genre", 60, B_STRING_TYPE);
		addAttribute(msg, "Album:Rating", "Rating", 40, B_INT32_TYPE);

		mime.SetAttrInfo(&msg);
	}

	if (installed) {
		installIcon(mime, kLargeIconBits, B_LARGE_ICON);
		installIcon(mime, kSmallIconBits, B_MINI_ICON);

		// the short description
		mime.SetShortDescription("Album Folder");

		// set the default application
		mime.SetPreferredApp("application/x-vnd.Be-TRAK");
	}
}


//	#pragma mark -


SettingsWindow::SettingsWindow(BRect rect)
	: BWindow(rect, kSettingsTitle, B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_NOT_RESIZABLE | B_NOT_ZOOMABLE)
{
	rect = Bounds();

	BView *view = new BView(rect, NULL, B_FOLLOW_ALL, 0);
	view->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	AddChild(view);

	// determine font height
	font_height fontHeight;
	view->GetFontHeight(&fontHeight);
	int32 height = (int32)(fontHeight.ascent + fontHeight.descent + fontHeight.leading) + 5;
	rect.InsetBySelf(8, 8);

	rect.bottom = rect.top + height;
	fAllowDifferentArtists = new BCheckBox(rect, NULL, "Allow different artists in album", NULL);
	fAllowDifferentArtists->ResizeToPreferred();
	fAllowDifferentArtists->SetValue(gAllowDifferentArtists);
	view->AddChild(fAllowDifferentArtists);

	rect.OffsetBySelf(0, height + 8);
	fCreateCoverIcons = new BCheckBox(rect, NULL, "Set directory icon to cover thumbnail",
		new BMessage(kMsgCreateCoverIconsChanged));
	fCreateCoverIcons->ResizeToPreferred();
	fCreateCoverIcons->SetValue(gCreateCoverIcons);
	view->AddChild(fCreateCoverIcons);

	rect.OffsetBySelf(10, height + 5);
	fUseImageIcon = new BCheckBox(rect, NULL, "Take over icons from cover", NULL);
	fUseImageIcon->ResizeToPreferred();
	fUseImageIcon->SetValue(gUseImageIcon);
	fUseImageIcon->SetEnabled(gCreateCoverIcons);
	view->AddChild(fUseImageIcon);

	rect.OffsetBySelf(-10, height + 8);
	fRecursive = new BCheckBox(rect, NULL, "Recursively scan directories for albums", NULL);
	fRecursive->ResizeToPreferred();
	fRecursive->SetValue(gRecursive);
	view->AddChild(fRecursive);

	rect.OffsetBySelf(0, height + 8);
	fUseMediaKit = new BCheckBox(rect, NULL, "Use Media Kit for playing length if no attribute is present", NULL);
	fUseMediaKit->ResizeToPreferred();
	fUseMediaKit->SetValue(gUseMediaKit);
	view->AddChild(fUseMediaKit);

	// change the size of the window to be large enough for its contents
	ResizeTo(fUseMediaKit->Bounds().Width() + 16, rect.bottom + 8);
}


SettingsWindow::~SettingsWindow()
{
	gSettingsWindowPosition = Frame();

	gRecursive = fRecursive->Value() != 0;
	gUseMediaKit = fUseMediaKit->Value() != 0;
	gCreateCoverIcons = fCreateCoverIcons->Value() != 0;
	gUseImageIcon = fUseImageIcon->Value() != 0;
	gAllowDifferentArtists = fAllowDifferentArtists->Value() != 0;

	saveSettings();
}


void
SettingsWindow::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case kMsgCreateCoverIconsChanged:
			fUseImageIcon->SetEnabled(fCreateCoverIcons->Value() != 0);
			break;
	}
}


//	#pragma mark -


extern "C" void
process_refs(entry_ref directoryRef, BMessage *msg, void *)
{
	// The tracker version has slightly different defaults
	gCreateCoverIcons = true;
	if (modifiers() & B_SHIFT_KEY)
		gForce = true;

	readSettings();

	if (modifiers() & B_CONTROL_KEY) {
		// find other settings window and bring those into sight
		int32 i = 0;
		while (BWindow *window = be_app->WindowAt(i++)) {
			if (!strcmp(window->Title(), kSettingsTitle)) {
				gHasSeenSettings = true;

				if (window->IsHidden())
					window->Show();
				else
					window->Activate();
				return;
			}
		}

		// none there, so it's out duty to change that :)

		if (gHasSeenSettings || (new BAlert("Album Attributes",
			"You have pressed the control key while chosing the \"Album Folder Attribute\" add-on.\n\n"
			"This might have happened by accident or because you have carefully read the documentation "
			"and know that this brings up its settings window.\n\n"
			"Nonetheless, you may now decide what to do. You will see this message only once; next "
			"time, the settings window will pop up directly, so make sure you've read and understood "
			"this message :-)\n",
			"Set Album Folder Attributes", "Settings"))->Go() != 0) {
			SettingsWindow *window = new SettingsWindow(gSettingsWindowPosition);
			window->Show();

			status_t status;
			wait_for_thread(window->Thread(), &status);
			return;
		}
	}

	// first, check if the MIME type is already installed

	BMimeType mime(kAlbumMimeString);
	if (mime.InitCheck() != B_OK
		|| !mime.IsInstalled())
		registerFileType();

	entry_ref ref;
	int32 index;
	for (index = 0; msg->FindRef("refs", index, &ref) == B_OK; index ++) {
		BEntry entry(&ref);
		if (entry.InitCheck() == B_OK)
			handleDirectory(entry, 0);
	}

	if (index == 0) {
		BEntry entry(&directoryRef);
		if (entry.InitCheck() == B_OK)
			handleDirectory(entry, 0);
	}
}


void
printUsage(char *cmd)
{
	char *name = strrchr(cmd, '/');
	if (name == NULL)
		name = cmd;
	else
		name++;

	printf("Copyright (c) 2003-2004 pinc software.\n"
		"Usage: %s [-vrmfictds] <list of directories>\n"
		"  -v\tverbose mode\n"
		"  -r\tenter directories recursively\n"
		"  -m\tdon't use the media kit: retrieve song length from attributes only\n"
		"  -f\tforces updates even if the directories already have attributes\n"
		"  -i\tinstalls the extra application/x-vnd.Be-directory-album MIME type\n"
		"  -c\tfinds a cover image and set their thumbnail as directory icon\n"
		"  -t\tdon't use the thumbnail from the image, always create a new one\n"
		"  -d\tallows different artists in one album (i.e. for samplers, soundtracks, ...)\n"
		"  -s\tread options from standard settings file\n",
		name);
}


int
main(int argc, char **argv)
{
	BApplication app("application/x-vnd.pinc.albumattr");

	char *cmd = argv[0];

	if (argc == 1) {
		printUsage(cmd);
		return 1;
	}

	bool registerType = false;
	gFromShell = true;

	while (*++argv && **argv == '-') {
		for (int i = 1; (*argv)[i]; i++) {
			switch ((*argv)[i]) {
				case 'v':
					gVerbose = true;
					break;
				case 'r':
					gRecursive = true;
					break;
				case 'm':
					gUseMediaKit = false;
					break;
				case 'f':
					gForce = true;
					break;
				case 'i':
					registerType = true;
					break;
				case 'c':
					gCreateCoverIcons = true;
					break;
				case 't':
					gUseImageIcon = false;
					break;
				case 'd':
					gAllowDifferentArtists = true;
					break;
				case 's':
					readSettings();
					break;
				default:
					printUsage(cmd);
					return 1;
			}
		}
	}

	if (registerType)
		registerFileType();

	argv--;

	while (*++argv) {
		BEntry entry(*argv);

		if (entry.InitCheck() == B_OK)
			handleDirectory(entry, 0);
		else
			fprintf(stderr, "could not find \"%s\".\n", *argv);
	}
	return 0;
}
