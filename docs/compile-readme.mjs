// docs/compile-readme.js
import { marked } from 'marked';
import fs from 'fs';
import path from 'path';

// Define paths
const rootDir = path.resolve(process.cwd(), '..');
const docsDir = process.cwd();
const inputPath = path.resolve(rootDir, 'README.md'); // Points to root README.md
// Create dist folder if necessary
if (!fs.existsSync(path.resolve(docsDir, 'dist'))) {
    fs.mkdirSync(path.resolve(docsDir, 'dist'));
}
const outputPath = path.resolve(docsDir, 'dist', 'index.html');
const templatePath = path.resolve(docsDir, 'readme-template.html');

// Add anchor links to headings, like on GitHub
const renderer = {
  heading(text, depth) {
    const escapedText = text.toLowerCase().replace(/[^\w]+/g, '-');

    return `
            <h${depth}>
              <a name="${escapedText}" class="anchor" href="#${escapedText}">
                <span class="header-link">🔗</span>
              </a>
              ${text}
            </h${depth}>`;
  }
};
marked.use({ renderer });

try {
    // Read the Markdown content
    let markdown = fs.readFileSync(inputPath, 'utf8');

    // Remove "mdOnly" content
    const mdOnlyRegex = /<p class="mdOnly">.*?<\/p>/g;
    markdown = markdown.replace(mdOnlyRegex, '');

    // Replace YouTube URLs with video embeds
    const youtubeRegex = /^(https?:\/\/(?:www\.)?youtube\.com\/watch\?v=([\w-]+).*)$/gm;
    markdown = markdown.replace(youtubeRegex, (match, url, videoId) => {
        return `<div class="yt"><iframe
            src="https://www.youtube-nocookie.com/embed/${videoId}" 
            title="YouTube video player" 
            frameborder="0" 
            allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" 
            referrerpolicy="strict-origin-when-cross-origin" 
            allowfullscreen></iframe></div>`;
    });

    // Generate TOC from headings
    const tocHeadings = marked.lexer(markdown).filter(token => token.type === 'heading' && (token.depth === 2 || token.depth === 3));
    const tocTree = [];
    let currentH2 = null;

    tocHeadings.forEach(token => {
        const item = {
            text: token.text,
            slug: token.text.toLowerCase().replace(/[^\w]+/g, '-'),
            children: []
        };
        if (token.depth === 2) {
            currentH2 = item;
            tocTree.push(currentH2);
        } else if (token.depth === 3 && currentH2) {
            currentH2.children.push(item);
        }
    });

    function buildTocHtml(tree) {
        if (!tree || tree.length === 0) {
            return '';
        }
        let html = '<ul>';
        tree.forEach(item => {
            html += `<li><a href="#${item.slug}">${item.text}</a>`;
            if (item.children.length > 0) {
                html += buildTocHtml(item.children);
            }
            html += '</li>';
        });
        html += '</ul>';
        return html;
    }

    const tocHtml = buildTocHtml(tocTree);
    
    
    // Convert to HTML
    const htmlContent = marked(markdown);

    // Read the HTML template (for styling and structure)
    let template = fs.readFileSync(templatePath, 'utf8');

    // Insert the generated TOC and HTML into the template
    let finalHtml = template.replace('{toc}', tocHtml);
    finalHtml = finalHtml.replace('{content}', htmlContent);

    // Write the final HTML file to the dist folder
    fs.writeFileSync(outputPath, finalHtml);

    console.log(`Successfully compiled ${inputPath} to ${outputPath}`);

} catch (error) {
    console.error('Error during Markdown compilation:', error);
    process.exit(1);
}