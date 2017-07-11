#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <3ds.h>

Result http_download(const char *url, char *fileName) {
	Result ret=0;
	httpcContext context;
	char *newurl=NULL;
	//u8* framebuf_top;
	u32 statuscode=0;
	u32 contentsize=0, readsize=0, size=0;//, fbufsize=0;
	u8 *buf, *lastbuf;
	FILE* fileOut;
	
	//fbufsize = 240*400*3*2;

	printf("Downloading %s\n",url);
	gfxFlushBuffers();

	do {
		ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
		printf("return from httpcOpenContext: %"PRId32"\n",ret);
		gfxFlushBuffers();

		// This disables SSL cert verification, so https:// will be usable
		ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
		printf("return from httpcSetSSLOpt: %"PRId32"\n",ret);
		gfxFlushBuffers();

		// Enable Keep-Alive connections (on by default, pending ctrulib merge)
		// ret = httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
		// printf("return from httpcSetKeepAlive: %"PRId32"\n",ret);
		// gfxFlushBuffers();

		// Set a User-Agent header so websites can identify your application
		ret = httpcAddRequestHeaderField(&context, "User-Agent", "missingno123s-updater/1.0.0");
		printf("return from httpcAddRequestHeaderField: %"PRId32"\n",ret);
		gfxFlushBuffers();

		// Tell the server we can support Keep-Alive connections.
		// This will delay connection teardown momentarily (typically 5s)
		// in case there is another request made to the same server.
		ret = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");
		printf("return from httpcAddRequestHeaderField: %"PRId32"\n",ret);
		gfxFlushBuffers();

		ret = httpcBeginRequest(&context);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			return ret;
		}

		ret = httpcGetResponseStatusCode(&context, &statuscode);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			return ret;
		}

		if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
			if(newurl==NULL) newurl = malloc(0x1000); // One 4K page for new URL
			if (newurl==NULL){
				httpcCloseContext(&context);
				return -1;
			}
			ret = httpcGetResponseHeader(&context, "Location", newurl, 0x1000);
			url = newurl; // Change pointer to the url that we just learned
			printf("redirecting to url: %s\n",url);
			httpcCloseContext(&context); // Close this context before we try the next
		}
	} while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));

	if(statuscode!=200){
		printf("URL returned status: %"PRId32"\n", statuscode);
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -2;
	}

	// This relies on an optional Content-Length header and may be 0
	ret=httpcGetDownloadSizeState(&context, NULL, &contentsize);
	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return ret;
	}

	printf("reported size: %"PRId32"\n",contentsize);
	gfxFlushBuffers();

	// Start with a single page buffer
	buf = (u8*)malloc(0x1000);
	if(buf==NULL){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	do {
		//I REALLY FUCKING HOPE THIS WILL WORK
		/*
		framebuf_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
		memcpy(framebuf_top, buf, fbufsize);
		gfxFlushBuffers();
		gfxSwapBuffers();

		framebuf_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
		memcpy(framebuf_top, buf, fbufsize);
		gfxFlushBuffers();
		gfxSwapBuffers();
		*/
		
		// This download loop resizes the buffer as data is read.
		ret = httpcDownloadData(&context, buf+size, 0x1000, &readsize);
		size += readsize; 
		if (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING){
				lastbuf = buf; // Save the old pointer, in case realloc() fails.
				buf = realloc(buf, size + 0x1000);
				if(buf==NULL){ 
					httpcCloseContext(&context);
					free(lastbuf);
					if(newurl!=NULL) free(newurl);
					return -1;
				}
			}
	} while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);	

	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		free(buf);
		return -1;
	}

	// Resize the buffer back down to our actual final size
	lastbuf = buf;
	buf = realloc(buf, size);
	if(buf==NULL){ // realloc() failed.
		httpcCloseContext(&context);
		free(lastbuf);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	printf("downloaded size: %"PRId32"\n",size);
	gfxFlushBuffers();

	//if(size>(240*400*3*2))size = 240*400*3*2;
	//fbufsize = 240*400*3*2;
	
	/*
	framebuf_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	memcpy(framebuf_top, buf, fbufsize);
	gfxFlushBuffers();
	gfxSwapBuffers();

	framebuf_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	memcpy(framebuf_top, buf, fbufsize);
	gfxFlushBuffers();
	gfxSwapBuffers();
	gspWaitForVBlank();
	*/

	httpcCloseContext(&context);
	
	fileOut = fopen(fileName,"wb");
	if (fileOut) {
		fwrite(buf, size, 1, fileOut);
		printf("Wrote file!\n");
	} else {
		printf("Something wrong writing file.\n");
	}
	fclose(fileOut);
	
	free(buf);
	if (newurl!=NULL) free(newurl);

	return 0;
}

int main() {
	Result ret=0;
	gfxInitDefault();
	httpcInit(0); // Buffer size when POST/PUT.
	//char *URL = "http://alteregobot.me/hourlies/";


	consoleInit(GFX_BOTTOM,NULL);
	
	u32 kDown = hidKeysDown();
	if (kDown & KEY_B) {
		ret=http_download("http://alteregobot.me/hourlies/updaterer/updaterer.3dsx", "/3ds/updaterer/updaterer.3dsx");
		printf("return from http_download: %"PRId32"\n",ret);
		ret=http_download("http://alteregobot.me/hourlies/updaterer/updaterer.smdh", "/3ds/updaterer/updaterer.smdh");
		printf("return from http_download: %"PRId32"\n",ret);
	}

	ret=http_download("http://alteregobot.me/hourlies/Luma3DS/boot.firm", "/boot.firm");
	printf("return from http_download: %"PRId32"\n",ret);
	
	ret=http_download("http://alteregobot.me/hourlies/GodMode9/GodMode9.firm", "/luma/payloads/GodMode9.firm");
	printf("return from http_download: %"PRId32"\n",ret);
	
	gfxFlushBuffers();

	// Main loop
	//while (aptMainLoop()){
		gspWaitForVBlank();
		//hidScanInput();
		
		printf("\nDone!\n");
		// Your code goes here

		//u32 kDown = hidKeysDown();
		//if (kDown & KEY_START)
			//break; // break in order to return to hbmenu

		// Flush and swap framebuffers
		//gfxFlushBuffers();
		//gfxSwapBuffers();
	//}

	// Exit services
	httpcExit();
	gfxExit();
	return 0;
}
